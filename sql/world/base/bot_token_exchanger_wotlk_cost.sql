CREATE TABLE IF NOT EXISTS `bot_token_exchanger_wotlk_cost` (
    `reward_item_id` INT UNSIGNED NOT NULL,
    `required_item_id` INT UNSIGNED NOT NULL,
    `required_item_name` VARCHAR(255) NOT NULL,
    `required_count` INT UNSIGNED NOT NULL,
    `required_item_role` VARCHAR(32) NOT NULL DEFAULT 'unknown',
    PRIMARY KEY (`reward_item_id`, `required_item_id`),
    KEY `idx_required_item` (`required_item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
