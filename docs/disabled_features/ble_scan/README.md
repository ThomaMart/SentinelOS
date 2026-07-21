# BLE scan (désactivé)

Composant BLE (scan passif via NimBLE, classification IFF-lite via allowlist)
implémenté et testé sur le board réel, puis **désactivé** : `ble_hs_init`
plante systématiquement (`assert failed: ble_hs_init ble_hs.c:967/982
(rc == 0)`, `NimBLE: ble_hs_conn_init rc=6`) faute de RAM/IRAM suffisante une
fois combiné au reste du firmware déjà chargé (Wi-Fi STA + CSI + mode
promiscuous + LVGL + client HTTPS OTA + FATFS).

Contrairement à l'échec propre du composant `sdcard` (retourne une erreur,
ne crashe rien), celui-ci est un `assert()` interne à NimBLE -- un crash dur
et systématique, pas quelque chose qu'on peut intercepter proprement côté
application. Retenter l'init plus tard dans le cycle de vie du firmware
(après l'OTA/radar critiques au boot) ne change rien : le heap libre
observé (~29-30 Ko) reste insuffisant même après avoir réduit
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`/`MAX_BONDS`/`MAX_CCCDS` au minimum.

Déplacé hors de `firmware/components/` pour ne plus être compilé (évite
une dépendance sur `CONFIG_BT_ENABLED`, désactivé dans `sdkconfig`).
Gardé ici tel quel, pour référence, si une piste de réduction mémoire
plus poussée (ou un board avec plus de RAM libre) permet un jour de le
réactiver.
