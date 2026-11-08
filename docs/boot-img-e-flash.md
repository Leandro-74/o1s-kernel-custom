# Empacotamento e gravação do boot.img (o1s)

O kernel é gravado junto com o ramdisk stock da ROM. O `mkbootimg` reproduz o
cabeçalho original e o `avbtool` refaz o rodapé AVB com o mesmo salt.

## Repack

```sh
mkbootimg \
  --header_version 3 \
  --os_version 16.0.0 \
  --os_patch_level 2026-09 \
  --kernel out/arch/arm64/boot/Image \
  --ramdisk ramdisk \
  --cmdline "loop.max_part=7" \
  --output boot-resukisu.img

avbtool add_hash_footer \
  --image boot-resukisu.img \
  --partition_name boot \
  --partition_size 67108864 \
  --hash_algorithm sha256 \
  --salt e69b6c1aab78382b458b1ad020148b7e00b1a524c4f272844ebd80a17392e941 \
  --prop "com.android.build.boot.os_version:16" \
  --prop "com.android.build.boot.fingerprint:samsung/lineage_o1s/o1s:16/BP4A.251205.006/eng.mack:userdebug/test-keys"
```

O `vbmeta.img` da ROM já está com a verificação desabilitada (flags 3), então não é
preciso re-assinar o `vbmeta`.

## Verificação antes de gravar

```sh
unpack_bootimg --boot_img boot-resukisu.img --out check
cmp check/kernel out/arch/arm64/boot/Image
cmp check/ramdisk ramdisk
avbtool info_image --image boot-resukisu.img
```

Repackar o kernel e o ramdisk stock deve reproduzir a imagem original, o que valida
o pipeline.

## Gravação (ROM userdebug, sem fastboot)

A partição `boot` é A-only (`/dev/block/by-name/boot` -> `/dev/block/sda13`).

```sh
adb root
adb push boot-resukisu.img /data/local/tmp/boot-resukisu.img
adb shell sha256sum /data/local/tmp/boot-resukisu.img

adb shell 'dd if=/data/local/tmp/boot-resukisu.img of=/dev/block/by-name/boot bs=1M conv=fsync'
adb shell sync

# Conferir o hash relendo a partição antes de reiniciar
adb shell 'dd if=/dev/block/by-name/boot of=/data/local/tmp/boot-verify.img bs=1M'
adb pull /data/local/tmp/boot-verify.img
sha256sum boot-verify.img boot-resukisu.img
```

Só reinicie com os dois hashes iguais.

## Rollback

- Backup da partição stock do próprio aparelho, com hash idêntico ao `boot.img` do ZIP
  da ROM.
- Download mode + Heimdall: `heimdall flash --BOOT boot-stock.img`
- Alternativa no Windows: Odin com o `boot.img` dentro de um TAR.

## Userspace do BakaSU

O kernel é built-in, mas o `ksud` precisa existir em `/data/adb/ksud`. O manager deve
ter a mesma versão do driver (`35257`); com APK mais antigo o app mostra
"Manager update required" e não cria o userspace.

Instalação manual, com root:

```sh
# libksud.so e libadbroot.so extraídos do APK do manager
adb push libksud.so /data/local/tmp/libksud.so
adb push libadbroot.so /data/local/tmp/libadbroot.so
adb shell 'chmod 755 /data/local/tmp/libksud.so'
adb shell '/data/local/tmp/libksud.so install --libadbroot /data/local/tmp/libadbroot.so'
adb shell '/data/adb/ksud --version'
```

Depois do install, o kernel executa `ksud post-fs-data`, `services` e
`boot-completed` via `init.rc` injetado, sem precisar do `ksuinit` no ramdisk.

Para conceder root a um app, use a aba SuperUsuário do manager.

## `su` no `adb shell`

O UID `shell` (2000) não é liberado por padrão: em `kernel/core/init.c`,
`allow_shell` só é `true` quando `CONFIG_KSU_DEBUG` está ligado. Isso é proposital,
não é falha de integração.

Não é preciso recompilar o kernel para liberar. O `shell` aparece na lista de apps
do manager como `Shell`/`com.android.shell` e pode ser autorizado como qualquer
outro app:

1. Aba SuperUsuário, busque por `shell`.
2. Toque em `Shell` (`com.android.shell`).
3. Escolha `Personalizado` e ligue `SuperUsuário`.

A autorização fica em `/data/adb/ksu/.allowlist` e sobrevive a reboots:

```sh
adb unroot
adb shell id            # uid=2000(shell)
adb shell 'su -c id'    # uid=0(root) ... context=u:r:ksu:s0
```

O dmesg confirma o caminho do hook: `do_execveat_common su found` seguido de
`do_ksu_handle_execveat_sucompat su->sh!`.

Alternativa via kernel: trocar o padrão de `allow_shell` para `true` exige rebuild e
reflash, e libera root para qualquer `adb shell` sem confirmação. Só use se esse for
o comportamento desejado; `CONFIG_KSU_DEBUG` não é recomendado por ligar o modo debug.
