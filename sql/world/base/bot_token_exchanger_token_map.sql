CREATE TABLE IF NOT EXISTS `bot_token_exchanger_token_map` (
    `token_item_id` INT UNSIGNED NOT NULL,
    `reward_item_id` INT UNSIGNED NOT NULL,
    `token_name` VARCHAR(255) NOT NULL,
    `reward_name` VARCHAR(255) NOT NULL,
    `inventory_type` INT UNSIGNED NOT NULL,
    `allowable_class` INT NOT NULL,
    `extended_cost_id` INT UNSIGNED NOT NULL,
    `vendor_entry` INT UNSIGNED NOT NULL,
    `source_expansion` VARCHAR(16) NOT NULL DEFAULT 'TBC',
    `source_tier` VARCHAR(32) NULL,
    `source_raid` VARCHAR(64) NULL,
    `confidence` VARCHAR(32) NOT NULL DEFAULT 'runtime_dbc_verified',
    `status` VARCHAR(32) NOT NULL DEFAULT 'staged',
    `notes` TEXT NULL,
    PRIMARY KEY (`token_item_id`, `reward_item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
