# Validação do kernel o1s com BakaSU — 2026-11-08

Build testado: `#2 SMP PREEMPT`, clang 21.0.0 (`r563880c`), LTO/CFI, release
`5.4.302-android12-5.4-exynos-ga273fe61262c`.

## Boot

- `sys.boot_completed` = `1` após o reboot.
- `uname -r` = `5.4.302-android12-5.4-exynos-ga273fe61262c`.
- `/proc/version` confirma o build local (`#2`) e o clang 21.
- Nenhum panic, CFI failure, `BUG:` ou `Unable to handle` no dmesg.

## Módulos e compatibilidade

- `lsmod` = 271 módulos, o mesmo conjunto do baseline stock (nenhuma diferença).
- Nenhum `unknown symbol`, `disagrees about version` ou
  `module verification failed`.
- O release string idêntico ao stock é o que mantém o vermagic dos módulos de
  `vendor_dlkm` válido.

## BakaSU

- Driver: `35257`, `v4.2.0-rc3-516d3e87`, ABI aarch64, Built-in, uapi 6.
- Manager `35257` instalado e conectando ao driver (`Em execução`).
- Userspace em `/data/adb/ksud` (`ksud 4.2.0-rc3-86-g516d3e87`).
- `post-fs-data` e `boot-completed` disparados no boot; allowlist persistiu entre
  reboots.
- Root concedido ao app de teste `com.joeykrim.rootcheck` e confirmado:
  "Congratulations! Root access is properly installed on this device!".
- Hook manual de `execve` confirmado no dmesg:
  `do_execveat_common su found` -> `do_ksu_handle_execveat_sucompat su->sh!`.
- `su` no `adb shell` validado sem recompilar nada: `com.android.shell` (UID 2000)
  autorizado pela aba SuperUsuário, com a entrada gravada em
  `/data/adb/ksu/.allowlist`. Com `adb unroot`, `adb shell id` retorna
  `uid=2000(shell)` e `adb shell 'su -c id'` retorna `uid=0(root)` com
  `context=u:r:ksu:s0`, inclusive após reboot.

## Warnings

Três `WARNING`s aparecem no primeiro minuto. Para descartar regressão, o boot stock
foi gravado de volta, reiniciado e o dmesg limpo comparado:

| Warning | Stock | Custom |
| --- | --- | --- |
| `kernel/sched/sched.h:1122 update_stats_wait_end` | sim | sim |
| `mali_kbase_mem_linux.c:1238 kbase_mem_umm_map_attachment` | sim | sim |
| `mm/memcontrol.c:1317 mem_cgroup_update_lru_size` (`lru_size -1`) | sim | sim |

São avisos pré-existentes do kernel Samsung, não regressões deste build.

## Hardware (verificação rápida)

- SELinux `Enforcing`.
- Wi-Fi habilitado (`wlan0`, ClientModeManager ativo).
- 5 dispositivos de câmera enumerados.
- Áudio com speaker/earpiece registrados.
- Sensores, fingerprint e Bluetooth carregam como módulos, sem erro no dmesg.

## Rollback

Testado nos dois sentidos: stock -> custom -> stock -> custom, com hash da partição
`boot` conferido na leitura após cada gravação. O backup stock do aparelho tem hash
idêntico ao `boot.img` do ZIP da ROM.

## Testes manuais pendentes

- Chamada, SMS e dados móveis.
- Captura de foto/vídeo (frontal e traseira).
- Microfone, alto-falante e fone com áudio real.
- GPS, NFC, impressão digital e sensores na prática.
- USB/MTP, carregamento rápido e OTG.
- Suspender/retomar, deep sleep e térmicas.
- Uso contínuo por 24 h.
