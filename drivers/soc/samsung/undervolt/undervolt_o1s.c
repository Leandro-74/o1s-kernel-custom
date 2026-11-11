// SPDX-License-Identifier: GPL-2.0
/*
 * Runtime undervolt interface for the Exynos 2100 (Galaxy S21 / o1s).
 *
 * The voltage/frequency table (fvmap) and the ACPM access live inside
 * cmupmucal.ko, which is provided by the ROM. This module does not replace it:
 * it only uses the exported helpers to expose a small, stable sysfs interface
 * and to apply the defaults configured at build time.
 *
 * A positive value means undervolt. Internally the margin is sent to the ACPM
 * as a negative percentage.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "../cal-if/cmucal.h"
#include <soc/samsung/cal-if.h>

/*
 * Teto de seguranca. Os valores validados em uso real ficam bem abaixo disto;
 * acima daqui o aparelho pode reiniciar por watchdog. A escrita e recusada
 * antes de chegar ao ACPM.
 */
#define UV_MAX_PERCENT	10

struct uv_domain {
	const char	*name;
	int		margin_id;
	int		percent;
	int		vclk_id;
};

struct uv_attr {
	struct kobj_attribute	kattr;
	struct uv_domain	*domain;
};

static DEFINE_MUTEX(uv_lock);
static struct kobject *uv_kobj;

static struct uv_domain cpucl0_domain = {
	.name = "cpucl0",
	.margin_id = MARGIN_CPUCL0,
	.percent = CONFIG_SOC_EXYNOS2100_UV_CL0,
	.vclk_id = -ENODEV,
};

static struct uv_domain cpucl1_domain = {
	.name = "cpucl1",
	.margin_id = MARGIN_CPUCL1,
	.percent = CONFIG_SOC_EXYNOS2100_UV_CL1,
	.vclk_id = -ENODEV,
};

static struct uv_domain cpucl2_domain = {
	.name = "cpucl2",
	.margin_id = MARGIN_CPUCL2,
	.percent = CONFIG_SOC_EXYNOS2100_UV_CL2,
	.vclk_id = -ENODEV,
};

static struct uv_domain gpu_domain = {
	.name = "gpu",
	.margin_id = MARGIN_G3D,
	.percent = CONFIG_SOC_EXYNOS2100_UV_GPU,
	.vclk_id = -ENODEV,
};

static struct uv_domain *uv_domains[] = {
	&cpucl0_domain,
	&cpucl1_domain,
	&cpucl2_domain,
	&gpu_domain,
};

static int uv_apply(struct uv_domain *domain, int percent)
{
	if (domain->vclk_id < 0)
		return -ENODEV;

	cal_dfs_set_volt_margin(domain->vclk_id | ACPM_VCLK_TYPE, -percent);
	domain->percent = percent;

	return 0;
}

static void uv_resolve_vclk(struct uv_domain *domain)
{
	unsigned int size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	unsigned int i;

	for (i = 0; i < size; i++) {
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		if (!vclk)
			continue;
		if (vclk->margin_id == domain->margin_id) {
			domain->vclk_id = i;
			return;
		}
	}
}

#define UV_ATTR(_file, _domain)						\
static ssize_t _file##_show(struct kobject *kobj,			\
			    struct kobj_attribute *attr, char *buf)	\
{									\
	struct uv_attr *ua = container_of(attr, struct uv_attr,	kattr);	\
									\
	return scnprintf(buf, PAGE_SIZE, "%d\n", ua->domain->percent);	\
}									\
									\
static ssize_t _file##_store(struct kobject *kobj,			\
			     struct kobj_attribute *attr,		\
			     const char *buf, size_t count)		\
{									\
	struct uv_attr *ua = container_of(attr, struct uv_attr,	kattr);	\
	int percent, ret;						\
									\
	ret = kstrtoint(buf, 10, &percent);				\
	if (ret)							\
		return ret;						\
	if (percent < 0 || percent > UV_MAX_PERCENT)			\
		return -EINVAL;						\
									\
	mutex_lock(&uv_lock);						\
	ret = uv_apply(ua->domain, percent);				\
	mutex_unlock(&uv_lock);						\
									\
	return ret ? ret : count;					\
}									\
									\
static struct uv_attr _file##_attr = {					\
	.kattr = __ATTR(_file##_uv_percent, 0600, _file##_show,		\
			_file##_store),					\
	.domain = _domain,						\
}

UV_ATTR(cpucl0, &cpucl0_domain);
UV_ATTR(cpucl1, &cpucl1_domain);
UV_ATTR(cpucl2, &cpucl2_domain);
UV_ATTR(gpu, &gpu_domain);

static struct attribute *uv_attrs[] = {
	&cpucl0_attr.kattr.attr,
	&cpucl1_attr.kattr.attr,
	&cpucl2_attr.kattr.attr,
	&gpu_attr.kattr.attr,
	NULL,
};

static const struct attribute_group uv_group = {
	.attrs = uv_attrs,
};

static int __init undervolt_init(void)
{
	int i, found = 0;

	for (i = 0; i < ARRAY_SIZE(uv_domains); i++) {
		uv_resolve_vclk(uv_domains[i]);
		if (uv_domains[i]->vclk_id < 0) {
			pr_warn("undervolt: domain %s not found\n",
				uv_domains[i]->name);
			continue;
		}
		found++;
	}

	if (!found)
		return -ENODEV;

	uv_kobj = kobject_create_and_add("exynos_uv", kernel_kobj);
	if (!uv_kobj)
		return -ENOMEM;

	if (sysfs_create_group(uv_kobj, &uv_group)) {
		kobject_put(uv_kobj);
		return -ENOMEM;
	}

	mutex_lock(&uv_lock);
	for (i = 0; i < ARRAY_SIZE(uv_domains); i++) {
		if (uv_domains[i]->percent)
			uv_apply(uv_domains[i], uv_domains[i]->percent);
	}
	mutex_unlock(&uv_lock);

	pr_info("undervolt: ready (cpucl0=%d%%, cpucl1=%d%%, cpucl2=%d%%, gpu=%d%%)\n",
		cpucl0_domain.percent, cpucl1_domain.percent,
		cpucl2_domain.percent, gpu_domain.percent);

	return 0;
}

static void __exit undervolt_exit(void)
{
	int i;

	mutex_lock(&uv_lock);
	for (i = 0; i < ARRAY_SIZE(uv_domains); i++) {
		if (uv_domains[i]->vclk_id >= 0)
			cal_dfs_set_volt_margin(
				uv_domains[i]->vclk_id | ACPM_VCLK_TYPE, 0);
	}
	mutex_unlock(&uv_lock);

	sysfs_remove_group(uv_kobj, &uv_group);
	kobject_put(uv_kobj);
}

module_init(undervolt_init);
module_exit(undervolt_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Runtime undervolt interface for the Exynos 2100");
MODULE_AUTHOR("o1s-kernel-custom");
