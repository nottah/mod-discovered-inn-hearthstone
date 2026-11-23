CREATE TABLE IF NOT EXISTS `character_hearthstone_inns` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `guid` int unsigned NOT NULL,
  `inn_guid` bigint unsigned NOT NULL,
  `creature_entry` int unsigned NOT NULL,
  `area_id` int unsigned NOT NULL DEFAULT 0,
  `map_id` int unsigned NOT NULL,
  `pos_x` float NOT NULL,
  `pos_y` float NOT NULL,
  `pos_z` float NOT NULL,
  `pos_o` float NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_character_inn` (`guid`, `inn_guid`),
  KEY `idx_character` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
