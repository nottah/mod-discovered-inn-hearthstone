# Discovered Inn Hearthstone

This AzerothCore WotLK module lets a character add an innkeeper as a Hearthstone destination by talking to that innkeeper and selecting **Discover this inn for your Hearthstone**. Using the existing Hearthstone opens a paged list of that character's discovered inns.

Selecting an inn starts item 6948's real spell (8690). This keeps normal Hearthstone casting and cooldown behavior; the module changes only that cast's teleport destination. Existing innkeeper services, including setting the regular home bind, remain available.

## Install

1. Add this module to the server source and rebuild `worldserver`.
2. Apply `data/sql/characters/base/discovered_inns.sql` to the characters database.
3. Apply `data/sql/world/base/discovered_inn_hearthstone.sql` to the world database.
4. Restart the worldserver and test with a character that has the ordinary Hearthstone item.

The SQL table is per character and survives server restarts. Discovery is character-specific. This module does not grant GM access or create a new item.

## Compatibility and provenance

This is an unofficial community module for AzerothCore WotLK. It was developed in a Yu'Lon-managed local installation whose server source came from `mod-playerbots/azerothcore-wotlk` at commit `32fd3472aa74961c91cb9bc942ee3e0a44b36779`, with `mod-playerbots/mod-playerbots` at commit `ecc4a537a3b43056e412fae63fb4a4271849b472`. Yu'Lon manages the installation; it is not the source of this module or a modified core distribution. Compatibility with other AzerothCore revisions is not guaranteed.

This project is not affiliated with or endorsed by AzerothCore, the Playerbots project, Yu'Lon, Blizzard Entertainment, or World of Warcraft.

The original module code is licensed under the MIT License; see [`LICENSE`](LICENSE). The AzerothCore APIs and server remain under their own licenses.
