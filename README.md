<div align="center">

# 🛡️ SentinelOS

**Firmware embarqué temps réel pour ESP32, avec OTA sécurisée et architecture orientée cybersécurité embarquée.**

[![Platform](https://img.shields.io/badge/platform-ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF%20v6-blue)](https://github.com/espressif/esp-idf)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-informational)](https://www.freertos.org/)
[![UI](https://img.shields.io/badge/UI-LVGL%209-9146FF)](https://lvgl.io/)
[![Board](https://img.shields.io/badge/board-ESP32--2432S028R-lightgrey)](https://github.com/rzeldent/esp32-smartdisplay)
[![Status](https://img.shields.io/badge/status-active%20development-brightgreen)]()
[![Version](https://img.shields.io/badge/version-0.12.0-yellow)]()

</div>

---

## 📖 Sommaire

- [Présentation](#-présentation)
- [Matériel](#-matériel)
- [Architecture logicielle](#-architecture-logicielle)
- [Fonctionnalités](#-fonctionnalités)
- [OTA sécurisée — le cœur du projet](#-ota-sécurisée--le-cœur-du-projet)
- [Protocole de commande UART](#-protocole-de-commande-uart)
- [Radar Wi-Fi (CSI)](#-radar-wi-fi-csi)
- [RWR — détecteur d'attaque Wi-Fi](#-rwr--détecteur-dattaque-wi-fi)
- [Bus I2C](#-bus-i2c)
- [Détecteur Rogue AP / Evil Twin](#-détecteur-rogue-ap--evil-twin)
- [Diagnostic système : watchdog + core dump](#-diagnostic-système--watchdog--core-dump)
- [Stockage micro-SD](#-stockage-micro-sd)
- [Tests](#-tests)
- [Structure du dépôt](#-structure-du-dépôt)
- [Build & flash](#-build--flash)
- [Roadmap](#-roadmap)
- [Captures d'écran](#-captures-décran)

---

## 🎯 Présentation

**SentinelOS** est un firmware temps réel construit sur **ESP-IDF v6 / FreeRTOS** pour carte **ESP32-2432S028R** (écran tactile ILI9341 + XPT2046). C'est un projet portfolio pensé comme une **base firmware robuste et sécurisée** : architecture modulaire par composants, gestion d'état défensive, et surtout un pipeline de **mise à jour OTA (Over-The-Air) avec vérification d'intégrité et rollback automatique**.

L'objectif n'est pas de faire une app de démo jetable, mais de démontrer des pratiques qu'on retrouve sur du vrai firmware industriel/embarqué critique : séparation HAL/logique métier, machine à états pour les opérations sensibles, garde-fous contre un firmware corrompu ou malveillant, observabilité système (heap, uptime, watchdog applicatif).

## 🔧 Matériel

| Composant | Détail |
|---|---|
| **MCU** | ESP32 (dual-core Xtensa LX6) |
| **Board** | ESP32-2432S028R ("Cheap Yellow Display") |
| **Écran** | LCD ILI9341, 320×240, SPI |
| **Tactile** | XPT2046 (résistif, SPI) |
| **Connectivité** | Wi-Fi 802.11 b/g/n |
| **Stockage** | Flash SPI — partitions OTA double-banque + core dump + SPIFFS ; micro-SD (FATFS/SPI, non fonctionnel sur ce board) |

## 🏗️ Architecture logicielle

Firmware organisé en **composants ESP-IDF indépendants**, chacun avec une responsabilité unique et une API C minimale exposée via `.h` :

```
main/
 └── app.c              orchestration au boot (app_main)

components/
 ├── bsp/                Board Support Package : LCD, LVGL, touch, backlight
 ├── config/             Configuration centralisée (Wi-Fi, versions, endpoints)
 ├── system/             Monitoring runtime : uptime, heap, version
 ├── storage/            Persistance clé/valeur sur NVS
 ├── wifi/               Gestion station Wi-Fi (init, connect, état)
 ├── ota/                ⭐ Update OTA sécurisée (manifest, download, SHA-256, rollback)
 ├── protocol/           Protocole de commande binaire framé (UART, CRC16, IFF HMAC)
 ├── display/            Rendu LVGL (menu, pages plein écran, progress OTA)
 ├── radar/              Radar Wi-Fi CSI (présence/mouvement)
 ├── rwr/                Détection deauth/disassoc Wi-Fi (mode promiscuous)
 ├── rogue_ap/           Détecteur rogue AP / evil twin (scan Wi-Fi actif)
 ├── i2c/                Bus I2C master + scan de périphériques
 ├── sdcard/             Stockage micro-SD (FATFS/SPI)
 ├── diag/               Diagnostic reset/crash, watchdog applicatif, core dump
 └── tasks/              Tâches FreeRTOS (legacy, non câblées dans app_main)
```

**Principe directeur :** chaque composant expose une API stable et cache son implémentation — `app_main()` ne fait qu'orchestrer des appels `xxx_init()` / `xxx_start()`, aucune logique métier n'y traîne. Ça permet de faire évoluer un composant (ex: passer d'un OTA "hash seul" à un OTA "signature cryptographique") sans toucher au reste du firmware.

## ⚙️ Fonctionnalités

- 🖥️ **Interface graphique tactile LVGL** (thème terminal vert/noir) : écran menu + pages plein écran (OTA, Security, Radar, RWR, I2C, Wi-Fi Scan, SD Card, Log), navigation par bouton uniquement
- 📡 **Radar Wi-Fi (CSI)** : détection de présence/mouvement par variance du signal ambiant, sans caméra (détaillé ci-dessous)
- 🚨 **RWR (Radar Warning Receiver)** : détection de rafales deauth/disassoc Wi-Fi en mode promiscuous (détaillé ci-dessous)
- 🔗 **Bus I2C** : scan à la demande du header d'extension, liste les périphériques détectés (détaillé ci-dessous)
- 🕵️ **Détecteur Rogue AP / Evil Twin** : scan Wi-Fi actif, repère SSID dupliqué et sécurité faible (détaillé ci-dessous)
- 🛡️ **Watchdog applicatif + core dump** : tâches critiques surveillées par le Task Watchdog, capture automatique en flash au crash pour analyse post-mortem (détaillé ci-dessous)
- 🪪 **IFF challenge-response** (HMAC-SHA256) sur le protocole UART : l'hôte doit prouver qu'il connaît un secret partagé pour être classé FRIEND (détaillé ci-dessous)
- 💾 **Stockage micro-SD** (FATFS/SPI) pour journaliser les boots — implémenté et testé en échec de détection sur ce board précis, voir la section dédiée
- 📶 **Wi-Fi station** avec attente de connexion et timeout applicatif au démarrage
- 💾 **Stockage NVS** abstrait (string / u32) pour persister config et état applicatif
- 📊 **Monitoring système** : uptime, heap libre, heap minimum observé, version firmware, raison du dernier reset
- 🔄 **Tâches FreeRTOS dédiées** : supervision système, rendu display, logging — priorités et stacks isolées
- 🔐 **OTA sécurisée** avec intégrité SHA-256, authenticité par signature ECDSA et rollback automatique (détaillé ci-dessous)
- 🔌 **Protocole de commande binaire framé sur UART**, partagé avec la console de logs, parsing défensif avec resynchronisation (détaillé ci-dessous)

## 🔐 OTA sécurisée — le cœur du projet

Le composant `ota` implémente une **machine à états** (`IDLE → CONNECTING → DOWNLOADING → VERIFYING → FINISHED/FAILED`) pour piloter tout le cycle de mise à jour :

1. **Manifest-based update check** — `ota_check_update()` récupère un `manifest.json` (HTTPS) et compare la version distante à `SENTINELOS_VERSION` avant de déclencher quoi que ce soit.
2. **Téléchargement HTTPS** via `esp_https_ota`, avec suivi de progression réelle (0-100%) exploité par l'UI.
3. **Vérification d'intégrité SHA-256** de l'image téléchargée **avant** le bascule de partition de boot — en cas de mismatch, l'update est abandonnée et le firmware courant reste actif.
4. **Vérification d'authenticité par signature ECDSA (P-256)** — le manifest porte aussi une `signature` (DER, hex) calculée par la clé privée de release sur ce même hash. Le firmware embarque uniquement la **clé publique** (`ota_pubkey.h`) et vérifie la signature via mbedtls avant d'accepter l'update ; sans ça, un attaquant capable de modifier le manifest pourrait fournir un hash *et* un firmware cohérents entre eux mais malveillants — la signature ferme ce trou car il faudrait aussi la clé privée, qui ne quitte jamais le serveur de release (`UpdateServer`, hors dépôt Git). Signature générée avec `tools/sign_firmware.py`.
5. **Rollback automatique** — le firmware bascule sur une partition en attente de validation (`ota_0` / `ota_1`, double-banque). S'il ne s'auto-confirme pas valide (`ota_confirm_valid()`, appelé après un test minimal comme une connexion Wi-Fi réussie), le **bootloader ESP-IDF annule automatiquement l'update au reboot suivant** et revient sur la version précédente.
6. **Affichage de la progression** sur l'écran LCD pendant tout le processus, pour un retour visuel en conditions réelles.

> **Secure Boot V2 actif** : la signature OTA protège le canal de mise à jour, mais ne suffisait pas seule à empêcher un flash direct par USB d'un firmware non signé. C'est maintenant verrouillé au niveau matériel — voir [`docs/SECURE_BOOT.md`](docs/SECURE_BOOT.md) pour la procédure et le détail de ce que ça change concrètement (RSA-PSS, clé publique en eFuse, JTAG désactivé).

> **⚠️ Piège sur le calcul du SHA-256 du manifest :** un `sha256sum firmware.bin` classique **ne matchera jamais**. ESP-IDF ajoute un SHA-256 à la fin de chaque image (`hash_appended`), calculé sur le fichier **moins ses 32 derniers octets** — c'est cette valeur-là que le firmware recalcule et compare (`esp_partition_get_sha256` / `esp_image_get_metadata`), pas un hash du fichier entier. Pour obtenir le bon hash à mettre dans `manifest.json` :
> ```bash
> python3 -c "print(open('sentinel_os.bin','rb').read()[-32:].hex())"
> ```

## 🔌 Protocole de commande UART

Le composant `protocol` implémente un protocole binaire framé pour piloter le firmware depuis un hôte, **sur le même UART0 que la console de logs** (pas de câblage supplémentaire) :

```
MAGIC(4o: AA 55 C3 3C) | LEN u16 LE | TYPE(1o) | PAYLOAD(LEN o) | CRC16 u16 LE
```

- **CRC16/CCITT-FALSE** (poly `0x1021`) sur `TYPE + PAYLOAD` — trame corrompue = silencieusement rejetée.
- **Parsing octet par octet, défensif** : tout octet qui ne correspond pas à une trame valide (magic, longueur hors bornes, CRC invalide) est ignoré sans bloquer le flux — les lignes de log (`ESP_LOGI`/`ESP_LOGW`) continuent de s'afficher normalement, entrelacées avec les réponses binaires.
- **Longueur de payload bornée** (`PROTOCOL_MAX_PAYLOAD`) — une trame qui annonce une longueur excessive est rejetée avant même de toucher le buffer, pas de dépassement possible.
- **Commandes** : `PING` (test de vie), `GET_INFO` (version, uptime, heap libre/minimum), `OTA_CHECK` (déclenche `ota_check_update()` à la demande plutôt qu'au seul boot), `IFF_CHALLENGE`/`IFF_RESPONSE` (voir ci-dessous).
- Client de test hôte : `tools/uart_proto_client.py` (`ping` / `info` / `ota-check` / `iff`), qui scanne le flux série à la recherche du magic et ignore le reste.

**IFF (Identification Friend or Foe)** — challenge-response HMAC-SHA256 pour authentifier l'hôte connecté :

1. L'hôte envoie `CMD_IFF_CHALLENGE` ; le device génère un nonce aléatoire 16 octets (`esp_fill_random`), à usage unique, expirant après 30s.
2. L'hôte doit renvoyer `HMAC-SHA256(clé partagée, nonce)` via `CMD_IFF_RESPONSE` — calculé côté device avec l'API PSA (`psa_mac_compute`, la même que la vérification de signature OTA).
3. Le device répond `FRIEND` si le HMAC correspond, `UNKNOWN` sinon. Le nonce est invalidé après une seule réponse (matchée ou non) pour empêcher le rejeu.

```bash
python3 tools/uart_proto_client.py iff              # FRIEND (bonne clé)
python3 tools/uart_proto_client.py iff --wrong-key   # UNKNOWN (démo)
```

> ⚠️ La clé symétrique de démo (`iff_secret.h`) est codée en dur dans le firmware — volontairement documenté comme un choix de démo, pas un vrai schéma de production (contrairement à la signature OTA, où seule la clé **publique** vit sur le device). Une vraie implémentation IFF utiliserait un élément sécurisé ou un schéma asymétrique par device.

## 📡 Radar Wi-Fi (CSI)

Le composant `radar` exploite le **Channel State Information** natif de l'ESP32 (`esp_wifi_set_csi_rx_cb`/`esp_wifi_set_csi_config`) — la même technique de fond que les projets "WiFi sensing" (ex. [RuView](https://github.com/ruvnet/RuView)) : détecter une présence ou un mouvement à partir des variations du signal radio ambiant, sans caméra ni capteur dédié.

- Chaque trame CSI reçue est réduite à une **amplitude moyenne**, comparée à une moyenne glissante ("baseline" du signal au repos).
- L'écart à cette baseline donne un **niveau 0-100** ; un pic soutenu au-delà d'un seuil déclenche l'état "mouvement détecté", maintenu quelques secondes pour rester visible malgré le rafraîchissement UI à 1 Hz.
- Le CSI n'est capturé que sur du trafic adressé à cette station — une session **ping périodique (150 ms) vers la passerelle** (`radar_start_probing()`) génère un flux régulier pour un échantillonnage dense (~6-7 trames/s), plutôt que de dépendre du trafic sporadique de la connexion.
- Onglet **RADAR** dédié : barre de niveau, statut CLEAR/MOTION DETECTED, compteur d'échantillons en direct.

> Calibration validée sur banc réel : détection de mouvement basée sur l'écart brut CSI (`MOTION_DEVIATION_THRESHOLD`, indépendant de `DEVIATION_SCALE` qui ne pilote que l'affichage 0-100%), avec un lissage EMA du niveau affiché et un débounce (2 échantillons consécutifs) pour ignorer le bruit RF ambiant. Repos ~0-7% stable, main devant la carte déclenche fiablement "MOTION DETECTED".

## 🚨 RWR — détecteur d'attaque Wi-Fi

Le composant `rwr` (Radar Warning Receiver, par analogie avec le récepteur d'alerte radar des aéronefs militaires — ici appliqué à un vrai risque Wi-Fi plutôt qu'un missile) exploite le **mode promiscuous** de l'ESP32 pour surveiller les trames de management 802.11 sur le canal courant, sans dépendre du trafic de la connexion établie :

- Filtre au niveau driver (`esp_wifi_set_promiscuous_filter`, `WIFI_PROMIS_FILTER_MASK_MGMT`) — seules les trames de management remontent au callback, pas les données.
- Chaque trame est classée par son sous-type 802.11 (offset 0 de l'en-tête) ; les trames **deauthentication** (0x0C) et **disassociation** (0x0A) sont comptées — ce sont les deux types de trames envoyées en rafale par un outil comme `aireplay-ng` pour forcer une déconnexion (typiquement pour capturer un handshake WPA ou faire un déni de service ciblé).
- Détection par **fenêtre glissante** : au-delà de 5 trames deauth/disassoc en moins d'une seconde, l'état passe à "ATTACK DETECTED" (une déconnexion légitime isolée ne génère qu'une ou deux trames, jamais une rafale) — maintenu affiché quelques secondes pour rester visible.
- Adresse MAC source de la dernière trame suspecte affichée (extraite de l'en-tête 802.11, champ Address 2).
- Onglet **RWR** dédié : statut CLEAR/ATTACK DETECTED, compteurs deauth/mgmt total, dernière adresse MAC vue.

> Fonctionne en parallèle du radar CSI (même radio, deux exploitations différentes des trames reçues) sans configuration supplémentaire — actif dès que le Wi-Fi est initialisé, pas besoin d'être connecté à un AP.

## 🔗 Bus I2C

Le composant `i2c` initialise un bus I2C master (`driver/i2c_master.h`, nouvelle API ESP-IDF) sur le header d'extension non peuplé de l'ESP32-2432S028R (SCL=GPIO22, SDA=GPIO27, pull-ups internes activés) — pins libres de tout usage LCD/tactile sur ce board.

- `i2c_bus_scan()` sonde chaque adresse 7 bits valide (0x03-0x77) via `i2c_master_probe()` et construit la liste des périphériques qui répondent.
- Onglet **I2C SCAN** dédié : bouton SCAN déclenchant un scan à la demande (dans une tâche séparée pour ne pas geler le rendu LVGL), liste des adresses trouvées au format hexadécimal.
- Sans rien de branché sur le header, le scan retourne normalement 0 périphérique — la fonctionnalité est prête à détecter un capteur/RTC/EEPROM externe branché sur ce header.

## 🕵️ Détecteur Rogue AP / Evil Twin

Le composant `rogue_ap` lance un scan Wi-Fi actif (`esp_wifi_scan_start`, bloquant) et analyse les réseaux trouvés à la recherche de deux signatures classiques :

- **Même SSID diffusé par plusieurs BSSID différents** — usurpation probable d'un réseau existant ("evil twin").
- **Sécurité faible** (`OPEN` ou `WEP`).

Scan **à la demande** (bouton dédié, tâche séparée) plutôt que périodique — un scan actif force un saut de canal qui perturberait le radar CSI et le RWR s'il tournait en continu. Onglet **WIFI SCAN** dédié, réseaux suspects marqués `!` avec la raison.

## 🛡️ Diagnostic système : watchdog + core dump

Le composant `diag` analyse `esp_reset_reason()` à chaque boot et classe le dernier reset : anormal (panic, watchdog tâche/interruption, brownout) ou normal (power-on, reset externe, reset logiciel). Un compteur de crashs et un compteur de boots sont persistés en NVS. Affiché sur l'onglet **SECURITY**.

- **Watchdog applicatif** : les deux tâches qui tournent en continu (`display_task`, `protocol_task`) s'inscrivent au Task Watchdog Timer (`esp_task_wdt_add`) et le nourrissent à chaque tour de boucle — un blocage (deadlock LVGL, boucle UART figée) déclenche un panic + reboot au lieu de figer le firmware indéfiniment sans recours.
- **Core dump vers flash** (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`) : partition dédiée de 64KB, capture automatique de l'état au crash, analysable après coup avec `idf.py coredump-info`. `diag_has_coredump()` détecte sa présence au boot suivant.

## 💾 Stockage micro-SD

Le composant `sdcard` monte une carte micro-SD en FATFS via SPI (`esp_vfs_fat_sdspi_mount`), en partageant le bus SPI déjà utilisé par l'écran LCD — l'ESP32 original n'a que deux contrôleurs SPI généraux, tous deux déjà pris par l'écran et le tactile. API : montage, capacité/espace libre, ajout de lignes horodatées à un fichier de log. Onglet **SD CARD** dédié.

> ⚠️ **Non vérifié sur ce board précis.** Testé avec une carte physiquement insérée : échec de détection à l'étape SD `CMD8` ("send_if_cond"), identique sur deux configurations de bus différentes (LCD et tactile) avec `CS=GPIO5`. Le slot "TF" du board n'a aucun label de pin lisible sur le PCB (juste des références composants près du connecteur, pas de datasheet disponible), et le vrai pinout n'a pas pu être confirmé sans schéma ni multimètre. Le code gère l'échec proprement (pas de crash, juste indisponible) et reste prêt à fonctionner si le bon pinout est identifié.

## ✅ Tests

Le parsing du protocole (`protocol_parser.c`) est séparé du dispatch des commandes (`protocol.c`) précisément pour être testable **sans dépendre du Wi-Fi/OTA/display** — un test unitaire n'a besoin de compiler qu'un seul fichier pur.

```bash
cd firmware/components/protocol/test_apps/protocol
idf.py set-target esp32
idf.py -p /dev/ttyUSB0 flash monitor
```

6 cas (framework Unity, exécutés sur la carte) : vecteur CRC-16/CCITT-FALSE officiel, trame valide avec/sans payload, bruit avant une trame (resynchronisation), longueur annoncée hors bornes, CRC corrompu — et dans chaque cas de rejet, vérification que le parseur se rétablit correctement sur la trame suivante.

> ⚠️ Ce test app a sa propre table de partitions (mono-slot `factory`, pas d'OTA) — il **remplace temporairement** le firmware flashé. Reflasher l'app principale (`cd firmware && idf.py flash`) après coup pour revenir à SentinelOS.

## 📁 Structure du dépôt

```
SentinelOS/
├── firmware/
│   ├── main/                  # point d'entrée app_main()
│   ├── components/            # composants métier (voir architecture ci-dessus)
│   ├── managed_components/    # dépendances gérées par l'IDF Component Manager
│   ├── third_party/           # BSP carte ESP32-2432S028R (submodule)
│   ├── partitions.csv         # table de partitions (double OTA + SPIFFS)
│   └── sdkconfig(.defaults)   # configuration ESP-IDF
├── tools/
│   ├── run.py                   # script de build/flash/monitor (fullclean→build→flash→monitor)
│   ├── sign_firmware.py         # signe un .bin (ECDSA P-256) et met à jour manifest.json
│   └── uart_proto_client.py     # client de test du protocole UART (ping/info/ota-check)
└── README.md
```

## 🚀 Build & flash

Prérequis : [ESP-IDF v6](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) installé (le script attend `~/esp/esp-idf-v6/export.sh`).

```bash
python3 tools/run.py
```

Ce script enchaîne automatiquement `idf.py fullclean`, `build`, `flash` puis `monitor` dans l'environnement ESP-IDF.

Ou manuellement :

```bash
cd firmware
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Secure Boot actif** : le bootloader n'est plus flashé automatiquement par `idf.py flash` (nécessaire une seule fois via `idf.py bootloader-flash` — déjà fait sur ce board, voir [`docs/SECURE_BOOT.md`](docs/SECURE_BOOT.md)). Le build signe automatiquement bootloader et app avec la clé pointée par `CONFIG_SECURE_BOOT_SIGNING_KEY` dans `sdkconfig` (chemin absolu propre à cette machine). Sans cette clé privée exacte, un firmware buildé depuis ce dépôt cloné ailleurs ne sera pas accepté par cette puce précise — normal, c'est tout l'intérêt du mécanisme.

## 🗺️ Roadmap

- [x] Architecture multi-composants + FreeRTOS multitâche
- [x] BSP écran/tactile + rendu LVGL
- [x] Stockage NVS abstrait
- [x] Wi-Fi station
- [x] OTA : download HTTPS + manifest + progress UI
- [x] OTA : rollback automatique (double partition)
- [x] OTA : vérification d'intégrité SHA-256
- [x] 🔏 Signature cryptographique du firmware (ECDSA P-256) avant flash
- [x] 🔒 Secure Boot ESP-IDF V2 (chaîne de confiance matérielle, RSA-PSS) — [activé](docs/SECURE_BOOT.md)
- [ ] 🔑 Chiffrement de la flash (AES-256) — [tenté, échec eFuse, abandonné sur ce board](docs/FLASH_ENCRYPTION.md)
- [x] 📡 Protocole de communication structuré (UART, framé, CRC16, parsing défensif)
- [x] 🧪 Tests de validation système (Unity, sur cible réelle — parser protocole)
- [x] 📶 Radar Wi-Fi CSI — détection présence/mouvement, interface tactile 6 onglets
- [x] 🚨 RWR — détection de rafales deauth/disassoc Wi-Fi (mode promiscuous)
- [x] 🔗 Bus I2C — scan à la demande du header d'extension (SPI/I2C/CAN/Ethernet)
- [x] 🕵️ Détecteur Rogue AP / Evil Twin — scan Wi-Fi actif, SSID dupliqué + sécurité faible
- [x] 🛡️ Watchdog applicatif + core dump vers flash (diagnostic reset/crash persisté)
- [x] 🪪 IFF challenge-response (HMAC-SHA256) sur le protocole UART
- [~] 💾 Stockage micro-SD (FATFS/SPI) — implémenté, [détection carte non fonctionnelle sur ce board](#-stockage-micro-sd)
- [ ] 📶 Scanner BLE (IFF-lite) — [implémenté, plante par manque de RAM une fois combiné au reste du firmware, désactivé](docs/disabled_features/ble_scan/README.md)

## 📸 Photo

<img width="1086" height="1448" alt="sentinelOS" src="https://github.com/user-attachments/assets/50dcc798-ea23-4cd0-aa69-0f8bdd8bf5f0" />

---

<div align="center">

Projet personnel

</div>
