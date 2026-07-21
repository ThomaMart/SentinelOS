# Chiffrement de la flash — runbook (préparation, pas encore activé)

⚠️ **Comme Secure Boot, la dernière étape de cette procédure brûle des
eFuses de manière irréversible.** Rien dans ce dépôt ni dans le firmware
actuel n'active le chiffrement — le `sdkconfig` du projet reste
volontairement inchangé.

## Pourquoi, en plus de Secure Boot

Secure Boot (voir [`docs/SECURE_BOOT.md`](SECURE_BOOT.md)) garantit
l'**intégrité/authenticité** : seul du code signé par la bonne clé peut
s'exécuter. Il ne protège pas la **confidentialité** : n'importe qui avec
un accès physique peut aujourd'hui lire le contenu de la flash (dump SPI)
et en extraire le binaire en clair — code, chaînes, et surtout tout secret
qui y traînerait (identifiants Wi-Fi stockés en NVS, clés embarquées, etc.).

Le chiffrement de flash ferme ce trou-là : la flash est chiffrée au repos,
un dump SPI direct ne donne que du contenu illisible. Les deux mécanismes
sont complémentaires et normalement activés ensemble pour la protection
IP complète visée par "IP protection: secure boot, firmware signing,
encryption, integrity".

## Spécificités ESP32 (puce originale, pas S2/S3+)

Contrairement aux puces plus récentes (XTS-AES, tailles de clé
configurables), l'ESP32 original n'a qu'**un seul schéma** : AES-256,
avec un tweak dérivé de l'adresse flash. Pas de choix de mode AES à faire
ici (`SOC_FLASH_ENCRYPTION_XTS_AES_OPTIONS` n'existe pas sur cette cible).

## Pas de clé à générer nous-mêmes

Différence importante avec Secure Boot : là-bas, on doit détenir une clé
de signature pour pouvoir produire de futurs firmwares acceptés par la
puce. Ici, le cas d'usage standard (recommandé) est que **la puce génère
elle-même sa clé de chiffrement**, aléatoirement, au premier boot avec le
chiffrement actif, et la brûle dans un bloc eFuse **en lecture bloquée**
— personne, pas même nous, ne peut la relire ensuite. C'est justement ce
qui rend le mécanisme robuste : pas de clé à protéger côté serveur de
release, pas de risque de fuite de cette clé-là.

(Il existe un mode "clé pré-provisionnée" pour des flows de production en
série, hors sujet ici — un seul board de dev.)

## Ce qui devient irréversible, précisément

Le **premier boot** après un flash avec `SECURE_FLASH_ENC_ENABLED=y` :
le bootloader génère la clé, la burn dans l'eFuse (bloc en lecture
bloquée), chiffre en place le contenu de la flash existante, et burn
l'eFuse qui active définitivement le déchiffrement transparent en
lecture. Après ça, plus moyen de revenir à de la flash en clair sur
cette puce.

## Development vs Release mode

- **Development** (`SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT`) : le
  bootloader UART peut encore accepter de nouvelles écritures en clair
  (qu'il chiffre alors en place) — on garde la possibilité de reflasher
  par USB pendant qu'on itère. C'est le choix recommandé pour ce board de
  dev.
- **Release** (`SECURE_FLASH_ENCRYPTION_MODE_RELEASE`) : désactive
  définitivement le flash USB en clair. Plus aucune mise à jour possible
  autrement que par OTA (déjà chiffré/signé). Le niveau de verrouillage
  "produit fini" — mais plus aucune itération possible sur ce board
  ensuite.

## Procédure (à dérouler consciemment)

### 1. Configurer le projet (safe, réversible tant qu'on ne flashe pas)

```bash
cd firmware
idf.py menuconfig
```
- `Security features`:
  - `Enable flash encryption on boot` → `y`
  - `Enable usage mode` → `Development` (recommandé pour ce board)

Modifie `sdkconfig` — encore réversible (`git checkout sdkconfig` ou ne
pas flasher).

### 2. Build

```bash
idf.py build
```

### 3. ⚠️ Point de non-retour : premier flash + premier boot

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Ne lancer que si c'est bien la carte sur laquelle on accepte de perdre
le flash-en-clair. Si Secure Boot est aussi prévu, activer les deux en
même temps évite deux cycles de burn séparés (voir
`SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER` dans le Kconfig ESP-IDF).

### 4. Après activation

Le firmware continue de fonctionner normalement — le chiffrement est
transparent pour le code applicatif (lecture/écriture flash déchiffrée/
chiffrée automatiquement par le contrôleur). L'OTA existant (SHA-256 +
signature ECDSA) n'a besoin d'aucune modification.

## ⚠️ Tentative du 2026-07-20 : échec, abandonné sur ce board

Activation tentée avec `SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT`. Résultat :

```
I (760) efuse: BURN BLOCK1
I (773) efuse: BURN BLOCK1 - OK (write block == read block)
I (774) efuse: BURN BLOCK0
E (785) efuse: BURN BLOCK0 - ERROR (written bits != read bits)
W (785) efuse: BLOCK0: next retry to fix an error [1/3]...
E (814) efuse: Written data are incorrect
E (825) flash_encrypt: Error programming security eFuses (err=0xffffffff).
E (831) boot: Initialization of Flash Encryption key failed (-1)
```

- **BLOCK1** (clé AES-256) a brûlé correctement — clé présente, lecture bloquée, mais **orpheline** : sans les bits de contrôle de BLOCK0, elle ne sert à rien.
- **BLOCK0** (bits de contrôle, dont `FLASH_CRYPT_CNT` qui active réellement le mécanisme) a échoué après 3 tentatives de burn — `written bits != read bits`.
- Conséquence : `FLASH_CRYPT_CNT` reste à `0` (pair = chiffrement considéré **désactivé** par la puce, malgré la clé présente). Chaque boot suivant retombait sur `Invalid key state` → boucle de reboot infinie (ota_0 jamais bootable, fallback sur ota_1 vide → `No bootable app partitions`).
- **Aucun contenu flash n'a été réellement chiffré** — l'échec est survenu avant cette étape, donc pas de données illisibles à récupérer.
- **Récupération réussie** : `UART_DOWNLOAD_DIS` était resté à `False` (comme prévu, voir `docs/SECURE_BOOT.md`), donc l'accès USB/esptool restait ouvert. Reflash d'un bootloader+app buildé **sans** `SECURE_FLASH_ENC_ENABLED` (donc qui ne fait plus aucune vérification liée au chiffrement) → carte immédiatement de retour à un boot normal. Wi-Fi, OTA, protocole UART revérifiés fonctionnels après coup.

**Cause probable :** marge de programmation eFuse insuffisante sur cette écriture précise (BLOCK0 contient déjà plusieurs bits verrouillés par l'activation Secure Boot faite juste avant — `JTAG_DISABLE`, `ABS_DONE_1`, etc. — et le burn suivant du chiffrement a pu se heurter à une limite physique/tension plutôt qu'à un bug logiciel). Pas d'investigation plus poussée faite.

**Décision : chiffrement de flash abandonné sur ce board.** Retenter l'activation risquerait de gaspiller un nouveau bloc eFuse (BLOCK1 déjà "brûlé pour rien" une fois) pour le même résultat, ou pire. Si le besoin redevient réel, prévoir une carte neuve dédiée et, avant de retenter, comprendre précisément pourquoi ce burn spécifique de BLOCK0 a échoué (tension d'alim pendant le burn ? bug connu de cette révision de puce/IDF ? ordre des opérations avec Secure Boot ?).

## Statut actuel

- [x] Procédure documentée
- [x] Tentée le 2026-07-20 — **échec**, carte récupérée avec succès, chiffrement non actif
- [ ] ~~Activation~~ — abandonné sur ce board, voir ci-dessus
