DELETE FROM `spell_script_names`
WHERE `spell_id` = 8690
  AND `ScriptName` = 'spell_discovered_inn_hearthstone';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (8690, 'spell_discovered_inn_hearthstone');
