#!/system/bin/sh
# Carrega o modulo de kernel do undervolt depois que a ROM carregou o
# cmupmucal.ko, de quem o modulo depende para falar com o ACPM.

MODDIR=${0%/*}

# O cmupmucal.ko e carregado pela ROM; espera o sysfs dele aparecer.
for _ in $(seq 1 30); do
    [ -d /sys/kernel/percent_margin ] && break
    sleep 1
done

lsmod | grep -q '^cmupmucal ' || exit 0

# Nao carrega duas vezes.
lsmod | grep -q '^undervolt_o1s ' && exit 0

insmod "$MODDIR/undervolt_o1s.ko"
