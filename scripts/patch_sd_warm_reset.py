"""Patch Arduino-ESP32 2.x SD warm-reset handling for ready SDHC cards.

Some cards retain READY state across an ESP-only reset. Their valid CMD58 OCR
response has R1=0 and OCR bits BUSY+CCS set. Arduino 2.0.17's legacy/CMD8
fallback accepts only R1=1, so FatFs receives STA_NOINIT even though the card
is responding correctly. Keep this narrowly guarded: newer/different sources
are left untouched and fail the build with an actionable message.
"""

from pathlib import Path
Import("env")

framework_dir = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
driver = framework_dir / "libraries" / "SD" / "src" / "sd_diskio.cpp"

old = """    } else {
        if (sdTransaction(pdrv, READ_OCR, 0, &resp) != 1 || !(resp & (1 << 20))) {
            log_w(\"READ_OCR failed: %X\", resp);
            goto unknown_card;
        }

        start = millis();
        do {
            token = sdTransaction(pdrv, APP_OP_COND, 0x100000, NULL);
        } while (token == 0x01 && (millis() - start) < 1000);

        if (!token) {
            card->type = CARD_SD;
        } else {
            start = millis();
            do {
                token = sdTransaction(pdrv, SEND_OP_COND, 0x100000, NULL);
            } while (token != 0x00 && (millis() - start) < 1000);

            if (token == 0x00) {
                card->type = CARD_MMC;
            } else {
                log_w(\"SEND_OP_COND failed: %u\", token);
                goto unknown_card;
            }
        }
    }
"""

new = """    } else {
        token = sdTransaction(pdrv, READ_OCR, 0, &resp);
        // A still-powered SDHC may already be READY after an ESP-only reset.
        // R1=0 plus valid voltage, BUSY and CCS bits is a usable SDHC state,
        // not a failed legacy-card initialization.
        if (token == 0 && (resp & (1 << 20)) &&
            (resp & (1UL << 31)) && (resp & (1UL << 30))) {
            card->type = CARD_SDHC;
            log_w(\"warm-reset SDHC accepted from OCR: %X\", resp);
        } else {
            if (token != 1 || !(resp & (1 << 20))) {
                log_w(\"READ_OCR failed: token=%u OCR=%X\", token, resp);
                goto unknown_card;
            }

            start = millis();
            do {
                token = sdTransaction(pdrv, APP_OP_COND, 0x100000, NULL);
            } while (token == 0x01 && (millis() - start) < 1000);

            if (!token) {
                card->type = CARD_SD;
            } else {
                start = millis();
                do {
                    token = sdTransaction(pdrv, SEND_OP_COND, 0x100000, NULL);
                } while (token != 0x00 && (millis() - start) < 1000);

                if (token == 0x00) {
                    card->type = CARD_MMC;
                } else {
                    log_w(\"SEND_OP_COND failed: %u\", token);
                    goto unknown_card;
                }
            }
        }
    }
"""

source = driver.read_text()
if "warm-reset SDHC accepted from OCR" not in source:
    if old not in source:
        raise RuntimeError(
            "Unsupported Arduino SD driver layout; warm-reset patch was not applied"
        )
    driver.write_text(source.replace(old, new, 1))
    print("Applied guarded Arduino SD warm-reset compatibility patch")

