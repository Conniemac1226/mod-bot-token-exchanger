CREATE TABLE IF NOT EXISTS `bot_token_exchanger_bot_preference` (
    `bot_guid` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `bot_name` VARCHAR(64) NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `preferred_role` VARCHAR(32) NOT NULL,
    `notes` TEXT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
