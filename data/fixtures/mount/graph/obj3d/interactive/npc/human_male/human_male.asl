ON INIT {
 SETNAME [description_human_male]
 SETGROUP human_male_demo

 SETGORE ON
 PHYSICAL RADIUS 30

 SET_MATERIAL FLESH
 SET_ARMOR_MATERIAL METAL
 SET_STEP_MATERIAL Foot_metal_plate

 SETSTAREFACTOR 0.5

 SET_NPC_STAT life 20
 SET_NPC_STAT armor_class 20
 SET_NPC_STAT absorb 25
 SET_NPC_STAT damages 0
 SET_XP_VALUE 0

 ACCEPT
}

ON INITEND {
 LOADANIM WAIT    "human_male_idle"
 LOADANIM WALK    "human_male_run"
 LOADANIM RUN     "human_male_run"
 LOADANIM DIE     "human_male_death"

 LOADANIM ACTION1 "human_male_gathering"
 LOADANIM ACTION2 "human_male_power_up"
 LOADANIM ACTION3 "human_male_drink_potion"

 ACCEPT
}

ON DIE {
 TIMER KILL_LOCAL
 FORCEANIM DIE
 ACCEPT
}
