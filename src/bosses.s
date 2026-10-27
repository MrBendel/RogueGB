; The bosses' shared RAM (room.h BOSSES_ADDR): the area each boss's file puts its
; state in (tools/build.mjs BOSS_FILES). OVR overlays the files' parts instead of
; laying them end to end; the linker takes an area's flags from its first
; definition, so build.mjs links this file first.
	.area __BOSSES (REL,OVR)
