# Secure Boot V2 — runbook (préparation, pas encore activé)

⚠️ **Ce document décrit une procédure qui, à sa dernière étape, brûle des
eFuses de manière irréversible.** Rien dans ce dépôt ni dans le firmware
actuel n'active Secure Boot — le `sdkconfig` du projet reste volontairement
inchangé tant que cette procédure n'est pas suivie consciemment, sur une
carte dont on accepte le risque.

## Pourquoi Secure Boot, en plus de la signature OTA

La signature ECDSA ajoutée à l'OTA (`ota_verify_signature`, voir README)
protège le canal de mise à jour : un attaquant qui compromet le manifest ne
peut pas faire accepter un firmware non signé par le processus `ota_task`.

Elle ne protège **pas** contre un flash direct par USB/JTAG : n'importe qui
avec un accès physique au port peut aujourd'hui flasher n'importe quel
binaire, signé ou non — le bootloader ESP-IDF stock ne vérifie rien.

Secure Boot V2 ferme ce trou : la **ROM bootloader** (gravée, non
modifiable) vérifie la signature du 2nd-stage bootloader avant de
l'exécuter, et ce bootloader vérifie à son tour la signature de l'app avant
de la lancer. Résultat : un flash USB d'un binaire non signé par la bonne
clé ne boote simplement plus.

## Pourquoi V2 (pas V1) sur cette carte

Ce board est un ESP32 révision **3.1** (confirmé par les logs boot :
`Chip rev: v3.1`). Secure Boot V2 (schéma RSA-3072 + PSS, eFuse block
digest, clés révocables) est supporté à partir de la révision 3.0
(`CONFIG_ESP32_REV_MIN_FULL >= 300`), et c'est le schéma recommandé par
Espressif (V1 est l'ancien schéma ECDSA, encore supporté mais legacy).

Conséquence : activer V2 impose de relever `ESP32_REV_MIN_FULL` à 3.0 dans
la config du projet — ce build ne fonctionnera plus que sur des puces
ESP32 rev ≥ 3.0. Sans importance ici puisque c'est exactement la puce de
cette carte, mais à savoir si le binaire devait un jour tourner ailleurs.

## Ce qui devient irréversible, précisément

La clé privée signe bootloader + app à chaque build (comme la clé OTA,
mais elle ne quitte jamais non plus la machine de build/release). Ce
n'est **pas** ça qui est irréversible.

L'irréversible, c'est le **premier boot** d'un bootloader compilé avec
`SECURE_BOOT=y` : à ce moment-là, le bootloader lui-même calcule le digest
de la clé publique et **brûle les eFuses** `ABS_DONE_1` (verrouille Secure
Boot) et le bloc de clé. Après ça :
- Plus aucun flash non signé ne boote, plus jamais, sur cette puce précise.
- Si la clé privée est perdue, plus aucune mise à jour n'est possible non
  plus (ni OTA ni USB) — la carte reste fonctionnelle sur le dernier
  firmware flashé, mais figée dessus pour toujours.
- Aucune commande `espsecure` ne "désactive" Secure Boot après coup — c'est
  un fusible physique, pas un paramètre logiciel.

## Backup de la clé — à faire avant toute chose

```bash
cp /home/thomas/Projects/UpdateServer/keys/secure_boot_v2_signing_key.pem \
   <emplacement de sauvegarde hors de cette machine, ex. clé USB/coffre>
```

Sans cette clé (ou une copie), impossible de produire un firmware que la
carte acceptera de booter, une fois Secure Boot actif.

## Procédure (à dérouler consciemment, pas en un seul copier-coller)

### 1. Générer la clé (déjà fait)

```bash
espsecure generate-signing-key --version 2 --scheme rsa3072 \
    /home/thomas/Projects/UpdateServer/keys/secure_boot_v2_signing_key.pem
```
→ fait, clé dans `~/Projects/UpdateServer/keys/`, jamais dans ce dépôt Git.

### 2. Configurer le projet (safe, réversible tant qu'on ne flashe pas)

```bash
cd firmware
idf.py menuconfig
```
- `Component config → ESP32-Specific → Minimum Supported ESP32 Revision` → `Rev 3.0`
- `Security features`:
  - `Enable hardware Secure Boot in bootloader` → `y`
  - `Secure Boot version` → `Secure Boot V2`
  - `Sign binaries during build` → `y`
  - `Secure boot signing key` → chemin absolu vers
    `~/Projects/UpdateServer/keys/secure_boot_v2_signing_key.pem`

Ceci modifie `sdkconfig` — encore réversible (`git checkout sdkconfig` ou
juste ne pas flasher).

### 3. Build

```bash
idf.py build
```
Bootloader et app sont signés automatiquement pendant le build.

### 4. ⚠️ Point de non-retour : premier flash + premier boot

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

C'est CE flash + le boot qui suit qui brûle les eFuses. Ne lancer que
si :
- la clé privée est sauvegardée ailleurs que sur cette seule machine,
- c'est bien la carte sur laquelle on accepte de perdre le flash libre,
- pas de manip hâtive : relire ce document une dernière fois avant.

### 5. Après activation

Toute future update (OTA ou USB) doit être buildée avec le même
`sdkconfig` (clé de signature configurée) — sinon le device refuse de
booter l'image. Le pipeline OTA existant (signature ECDSA applicative +
Secure Boot matériel) devient alors une défense en profondeur à deux
niveaux indépendants.

## Statut actuel

- [x] Clé Secure Boot V2 générée, sauvegardée hors dépôt
- [x] Procédure documentée
- [x] `sdkconfig` du projet configuré (`SECURE_BOOT_V2_ENABLED`, `ESP32_REV_MIN_3`, table de partitions décalée à `0x10000` pour laisser la place au bootloader signé)
- [x] **Activé le 2026-07-20** — bootloader et app flashés signés, premier boot confirmé : `Secure boot permanently enabled`, clé publique burnée en eFuse, JTAG désactivé. UART download mode volontairement laissé actif (voir avertissement ESP-IDF au boot) pour garder la capacité de reflash USB avec des images signées. Device opérationnel, Wi-Fi/OTA/protocole UART tous vérifiés fonctionnels après coup.
- [ ] Toute future release doit être buildée+signée avec cette même clé (`~/Projects/UpdateServer/keys/secure_boot_v2_signing_key.pem`) — sinon la puce refuse l'image
