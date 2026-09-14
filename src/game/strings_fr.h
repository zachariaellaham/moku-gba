// French UI text. Order must match UI_STRING_LIST in strings_list.h (checked by a
// static_assert in strings.cpp). Limits per category: docs/decisions/strings.md.
#pragma once
// Rows are T(ID, text); same order as the id list.
#define UI_TEXT_FR(T) \
    /* Title screen (mockup 1a) */ \
    T(TITLE_TACTICS, "TACTIQUES DU GO") \
    T(MENU_CAMPAIGN, "CAMPAGNE") \
    T(MENU_SANDBOX, "PARTIE LIBRE") \
    T(MENU_OPTIONS, "OPTIONS") \
    T(MENU_CONTINUE, "REPRENDRE") \
    T(MSG_TITLE_FOOTER, "©2026 · FICHIER") \
    T(MSG_DEBUG_UNLOCK, "Tout est débloqué.") \
    /* Save select */ \
    T(TITLE_SAVE_SELECT, "CHOISIS TON FICHIER") \
    T(HUDL_FILE, "FICHIER") \
    T(VAL_EMPTY, "VIDE") \
    T(VAL_NEW_GAME, "NOUVELLE") \
    T(MENU_ERASE, "EFFACER") \
    T(HUDL_MISSIONS, "MISSIONS") \
    T(HUDL_STARS, "ÉTOILES") \
    T(HUDL_TIME, "TEMPS") \
    T(MSG_ERASE_CONFIRM, "Effacer ce fichier ?\nToute la progression sera perdue.") \
    T(MENU_YES, "OUI") \
    T(MENU_NO, "NON") \
    T(MSG_SAVED, "Progression enregistrée.") \
    T(MSG_SAVE_FAILED, "Écriture SRAM impossible.") \
    T(NAV_SAVE_SELECT, "A CHOISIR · B RETOUR") \
    /* Campaign map (mockup 1b) */ \
    T(HUDL_MISSION, "MISSION") \
    T(CHAP_1, "CH.1 BASES") \
    T(CHAP_2, "CH.2 CAPTURE") \
    T(CHAP_3, "CH.3 LA VIE") \
    T(CHAP_4, "CH.4 LA FORME") \
    T(CHAP_5, "CH.5 LA PARTIE") \
    T(VAL_LOCKED, "VERROUILLÉ") \
    T(MSG_LOCKED, "Termine la mission d’avant\npour ouvrir celle-ci.") \
    T(HINT_MAP_MORE, "11-20 →") \
    T(NAV_MAP, "A JOUER · B RETOUR") \
    /* Briefing (mockups 1c / 2d) */ \
    T(HUDL_OBJECTIVE, "OBJECTIF") \
    T(HUDL_RANK, "RANG") \
    T(WHO_SEN, "MAÎTRE SEN") \
    T(WHO_INDI, "INDI") \
    T(WHO_REX, "REX") \
    T(HINT_ADVANCE, "▼ A") \
    T(NAV_BRIEFING, "A JOUER · B RETOUR") \
    /* HUD labels (mockups 1d, 1e) */ \
    T(HUDL_TO_PLAY, "AU TRAIT") \
    T(HUDL_CAPT, "PRIS") \
    T(HUDL_MOVE, "COUP") \
    T(HUDL_KOMI, "KOMI") \
    T(HUDL_LIBERTIES, "LIBERTÉS :") \
    T(HUDL_INFO, "INFO") \
    T(HUDL_MOVES, "COUPS") \
    T(HUDL_ESTIMATE, "ESTIMATION") \
    T(HUDL_TERRITORY, "TERRITOIRE") \
    T(HUDL_PRISONERS, "PRISONNIERS") \
    T(HUDL_AREA, "ZONE") \
    T(HUDL_TOTAL, "TOTAL") \
    T(HUDL_RESULT, "RÉSULTAT") \
    T(HUDL_DEAD, "MORTES") \
    T(HUDL_CAPTURES, "CAPTURES") \
    T(HUDL_UNDOS, "ANNULÉS") \
    T(HUDL_HINTS, "INDICES") \
    T(HUDL_UNLOCKED, "DÉBLOQUÉ") \
    T(HUDL_LEVEL, "NIVEAU") \
    T(HUDL_RECORD, "TON BILAN") \
    /* Stone colours (per skin: classic / pup / dino) */ \
    T(VAL_BLACK, "NOIR") \
    T(VAL_WHITE, "BLANC") \
    T(VAL_BLUE, "BLEU") \
    T(VAL_ORANGE, "ORANGE") \
    T(VAL_OBSIDIAN, "OBSIDIENNE") \
    T(VAL_AMBER, "AMBRE") \
    T(VAL_LTR_BLACK, "N") \
    T(VAL_LTR_WHITE, "B") \
    T(VAL_LTR_BLUE, "B") \
    T(VAL_LTR_ORANGE, "O") \
    T(VAL_LTR_OBSIDIAN, "O") \
    T(VAL_LTR_AMBER, "A") \
    /* In-game button hints (mockup 1d bottom bar) */ \
    T(HINT_A_PLACE, "A POSER") \
    T(HINT_B_UNDO, "B ANNULER") \
    T(HINT_SEL_HINT, "SEL INDICE") \
    T(HINT_R_PASS, "R PASSER") \
    T(HINT_START_PAUSE, "START PAUSE") \
    T(HINT_A_CONFIRM, "A CONFIRMER") \
    T(HINT_B_CANCEL, "B ANNULER") \
    T(HINT_L_R_PANEL, "L R PANNEAU") \
    T(HINT_A_NEXT, "A ▶") \
    T(HINT_HINTS_SHOWN, "SEL INDICE · VUS") \
    T(HINT_MARK_DEAD, "A MARQUER") \
    T(HINT_MARK_DONE, "START FINI") \
    /* Event banners (mockups 1e, 2b, 2c). CAPTURE gets a runtime suffix: tr(BAN_CAPTURE) + " +N" */ \
    T(BAN_ATARI, "ATARI !") \
    T(BAN_ATARI_PUP, "OUPS ! ATARI") \
    T(BAN_ATARI_DINO, "GRRR ! ATARI") \
    T(BAN_CAPTURE, "PRISE !") \
    T(BAN_PASS, "PASSE") \
    T(BAN_KO, "KO !") \
    T(BAN_RESIGN, "ABANDON") \
    /* Atari hint line (mockup 1e). Scene appends " " + coordinate + "." */ \
    T(MSG_ATARI_BLACK, "Noir n’a qu’un souffle en") \
    T(MSG_ATARI_WHITE, "Blanc n’a qu’un souffle en") \
    /* Illegal moves */ \
    T(MSG_ILLEGAL_OCCUPIED, "Il y a déjà une pierre ici.") \
    T(MSG_ILLEGAL_SUICIDE, "Une pierre sans souffle ne\npeut pas être posée là.") \
    T(MSG_ILLEGAL_KO, "Ko : joue ailleurs d’abord.") \
    T(MSG_ILLEGAL_SUPERKO, "Cela répéterait toute la\nposition.") \
    /* Mission clear / fail (mockup 1f) */ \
    T(TITLE_MISSION_CLEAR, "MISSION RÉUSSIE") \
    T(TITLE_MISSION_FAIL, "MISSION MANQUÉE") \
    T(MENU_RETRY, "RÉESSAYER") \
    T(MENU_NEXT, "MISSION SUIV.") \
    T(MENU_MAP, "CARTE") \
    T(MSG_STARS_EARNED, "Rang A ou mieux, sans indice\nni annulation.") \
    /* Unlock messages, in UNLOCK_* order (missions.h): index with MSG_UNLOCK_SANDBOX_9_EASY + id - 1 */ \
    T(MSG_UNLOCK_SANDBOX_9_EASY, "Partie libre · 9×9 · IA FACILE") \
    T(MSG_UNLOCK_NORMAL, "Partie libre · KOAN, IA NORMALE") \
    T(MSG_UNLOCK_13, "Partie libre · plateau 13×13") \
    T(MSG_UNLOCK_HARD, "Partie libre · EMBER, IA FORTE") \
    T(MSG_UNLOCK_19_MASTER, "Partie libre · 19×19 + TENGEN") \
    /* Sandbox setup (mockup 1g) */ \
    T(TITLE_SANDBOX, "PARTIE LIBRE") \
    T(HUDL_BOARD, "PLATEAU") \
    T(HUDL_YOU_PLAY, "TU JOUES") \
    T(HUDL_AI, "IA") \
    T(HUDL_HANDICAP, "HANDICAP") \
    T(HUDL_SCORING, "COMPTAGE") \
    T(HUDL_SKIN, "STYLE") \
    T(HUDL_PLAYERS, "JOUEURS") \
    T(MENU_START_GAME, "COMMENCER") \
    T(VAL_AREA, "ZONE") \
    T(VAL_TERRITORY, "TERRITOIRE") \
    T(VAL_VS_AI, "CONTRE L’IA") \
    T(VAL_HOTSEAT, "2 JOUEURS") \
    T(VAL_PLAYER_1, "JOUEUR 1") \
    T(VAL_PLAYER_2, "JOUEUR 2") \
    T(MSG_PASS_DEVICE, "Passe la console à l’autre\njoueur.") \
    T(NAV_SANDBOX, "← → CHANGER · A JOUER · B RETOUR") \
    /* AI levels and their one-line descriptions (mockup 1g right column) */ \
    T(VAL_LVL_VERY_EASY, "TRÈS FACILE") \
    T(VAL_LVL_EASY, "FACILE") \
    T(VAL_LVL_NORMAL, "NORMALE") \
    T(VAL_LVL_HARD, "FORTE") \
    T(VAL_LVL_MASTER, "EXPERTE") \
    T(LVLB_VERY_EASY, "Joue au hasard.\nNe remplit pas\nses yeux.") \
    T(LVLB_EASY, "Prend toutes les\ncaptures. Sauve\nses pierres.") \
    T(LVLB_NORMAL, "Lit les formes\net le territoire\nà un coup.") \
    T(LVLB_HARD, "Explore des\nmilliers de\nfuturs. 2 s.") \
    T(LVLB_MASTER, "Recherche et\néchelles. 5 s\nde réflexion.") \
    /* Choose opponent (mockup 2a). Character names are proper nouns: identical in both languages. */ \
    T(TITLE_CHOOSE_OPPONENT, "CHOISIS L’ADVERSAIRE") \
    T(OPP_PEBBLE, "PEBBLE") \
    T(OPP_SPROUT, "SPROUT") \
    T(OPP_KOAN, "KOAN") \
    T(OPP_EMBER, "EMBER") \
    T(OPP_TENGEN, "TENGEN") \
    T(OPPT_PEBBLE, "LA TORTUE ENDORMIE") \
    T(OPPT_SPROUT, "LA POUSSE PRESSÉE") \
    T(OPPT_KOAN, "LE MOINE ERRANT") \
    T(OPPT_EMBER, "LE GÉNÉRAL RENARD") \
    T(OPPT_TENGEN, "LE VIEUX MAÎTRE") \
    T(BLURB_PEBBLE, "Joue presque au hasard. Mais\nne bouche jamais ses yeux.") \
    T(BLURB_SPROUT, "Saisit toutes les captures et\nsauve ses pierres en atari.") \
    T(BLURB_KOAN, "Lit les formes et le territoire\nà un coup. Solide et patient.") \
    T(BLURB_EMBER, "Explore des milliers de futurs.\nRéfléchit deux secondes.") \
    T(BLURB_TENGEN, "Recherche complète et lecture\ndes échelles. Cinq secondes.") \
    T(MSG_BEST_WIN_BY, "RECORD : GAIN DE") \
    T(MSG_NO_RECORD, "Aucune partie jouée.") \
    T(VAL_W, "V") \
    T(VAL_L, "D") \
    T(NAV_OPPONENT, "← → · A CONFIRMER") \
    /* Opponent speech bubbles (mockup 2b): intro, on capturing, on being captured, win, lose, atari taunt */ \
    T(SAY_PEBBLE_INTRO, "Oh… c’est à moi ?") \
    T(SAY_PEBBLE_CAPTURE, "J’en ai pris une ?") \
    T(SAY_PEBBLE_CAPTURED, "Bon… prends, prends.") \
    T(SAY_PEBBLE_WIN, "J’ai gagné ? Au dodo.") \
    T(SAY_PEBBLE_LOSE, "Belle partie. Zzz…") \
    T(SAY_PEBBLE_ATARI, "Euh… ça semble serré.") \
    T(SAY_SPROUT_INTRO, "On pousse ! À moi !") \
    T(SAY_SPROUT_CAPTURE, "Attrapée ! Attrapée !") \
    T(SAY_SPROUT_CAPTURED, "Hé ! Elle était à moi !") \
    T(SAY_SPROUT_WIN, "J’ai gagné ! Encore ?") \
    T(SAY_SPROUT_LOSE, "Ooh. On rejoue ?") \
    T(SAY_SPROUT_ATARI, "Un souffle ! Ouille !") \
    T(SAY_KOAN_INTRO, "La route est longue.") \
    T(SAY_KOAN_CAPTURE, "Les pierres retournent\nau bol.") \
    T(SAY_KOAN_CAPTURED, "Rien ne dure toujours.") \
    T(SAY_KOAN_WIN, "Le plateau dit le vrai.") \
    T(SAY_KOAN_LOSE, "Tu m’as instruit.") \
    T(SAY_KOAN_ATARI, "Un souffle reste un.") \
    T(SAY_EMBER_INTRO, "Brille fort, rival.") \
    T(SAY_EMBER_CAPTURE, "Au feu, ces pierres.") \
    T(SAY_EMBER_CAPTURED, "Une égratignure.") \
    T(SAY_EMBER_WIN, "Cendres. Reviens plus\nchaud.") \
    T(SAY_EMBER_LOSE, "Bien joué. Je brûlerai\nplus fort.") \
    T(SAY_EMBER_ATARI, "Un souffle. Ça sent le\nroussi.") \
    T(SAY_TENGEN_INTRO, "Montre-moi le centre.") \
    T(SAY_TENGEN_CAPTURE, "C’est ainsi.") \
    T(SAY_TENGEN_CAPTURED, "Bien. Tu as vu.") \
    T(SAY_TENGEN_WIN, "Joue mille parties.") \
    T(SAY_TENGEN_LOSE, "Le vieil arbre plie.") \
    T(SAY_TENGEN_ATARI, "Compte tes souffles.") \
    /* Options (mockup 2e) */ \
    T(TITLE_OPTIONS, "OPTIONS") \
    T(HUDL_LANGUAGE, "LANGUE") \
    T(HUDL_HELPER, "COMPAGNON") \
    T(HUDL_EFFECTS, "EFFETS") \
    T(HUDL_CONFIRM, "CONFIRMER") \
    T(HUDL_MUSIC, "MUSIQUE") \
    T(HUDL_SFX, "SONS") \
    T(VAL_LANG_EN, "EN") \
    T(VAL_LANG_FR, "FR") \
    T(VAL_SKIN_CLASSIC, "CLASSIQUE") \
    T(VAL_SKIN_PUP, "TOUTOU") \
    T(VAL_SKIN_DINO, "DINO") \
    T(VAL_EFFECTS_OFF, "SANS") \
    T(VAL_EFFECTS_JUICY, "À FOND") \
    T(VAL_CONFIRM_2A, "2×A") \
    T(VAL_CONFIRM_1A, "1×A") \
    T(MSG_HELPER_ROLE, "Commente les missions et\nles événements.") \
    T(MSG_HELPER_SEN, "Maître Sen. Calme, précis,\njamais pressé.") \
    T(MSG_HELPER_INDI, "Indi le chiot. Bruyant,\nfidèle, très excité.") \
    T(MSG_HELPER_REX, "Rex le petit rex. Toujours\naffamé de pierres.") \
    T(NAV_OPTIONS, "← → CHANGER · B RETOUR") \
    /* Helper interjections (bubble on events; also prefixes for tutor lines) */ \
    T(INTJ_SEN_ATARI, "Plus qu’un souffle.") \
    T(INTJ_SEN_CAPTURE, "Bien pris.") \
    T(INTJ_SEN_CLEAR, "Bien joué.") \
    T(INTJ_SEN_FAIL, "Respire. Recommence.") \
    T(INTJ_SEN_THINK, "Prends ton temps.") \
    T(INTJ_INDI_ATARI, "Oh ! Un seul souffle !") \
    T(INTJ_INDI_CAPTURE, "Bon chien ! Attrapé !") \
    T(INTJ_INDI_CLEAR, "OUAF ! Tu as réussi !") \
    T(INTJ_INDI_FAIL, "Oh… on réessaie !") \
    T(INTJ_INDI_THINK, "Je réfléchis !") \
    T(INTJ_REX_ATARI, "Un souffle. MORDS !") \
    T(INTJ_REX_CAPTURE, "CROC ! Délicieux.") \
    T(INTJ_REX_CLEAR, "RAAAH ! Victoire !") \
    T(INTJ_REX_FAIL, "Grr. Réessaie !") \
    T(INTJ_REX_THINK, "Hmm… raaah ?") \
    T(VOICE_INDI, "Ouaf !") \
    T(VOICE_REX, "Rrraah !") \
    /* Pause menu (mockup 1h) */ \
    T(TITLE_PAUSED, "PAUSE") \
    T(MENU_RESUME, "REPRENDRE") \
    T(MENU_UNDO, "ANNULER") \
    T(MENU_HINT, "INDICE") \
    T(MENU_PASS, "PASSER") \
    T(MENU_ESTIMATE, "SCORE ESTIMÉ") \
    T(MENU_RESIGN, "ABANDONNER") \
    T(MENU_QUIT, "QUITTER") \
    T(MSG_RESIGN_CONFIRM, "Abandonner la partie ?") \
    T(MSG_QUIT_CONFIRM, "Quitter la mission ?\nLa partie sera perdue.") \
    T(MSG_ESTIMATE_NOTE, "Simple estimation. Les pierres\nmortes sont devinées.") \
    T(MSG_NO_HINT, "Pas d’indice ici. Fie-toi à\nta lecture.") \
    T(MSG_NO_UNDO, "Rien à annuler.") \
    T(NAV_PAUSE, "START REPRENDRE · B RETOUR") \
    /* End of game: dead stone marking, then the score screen */ \
    T(TITLE_GAME_OVER, "FIN DE PARTIE") \
    T(TITLE_SCORE, "SCORE FINAL") \
    T(MSG_MARK_DEAD, "Deux passes. Marque les\npierres mortes, puis START.") \
    T(MSG_STUCK, "Bloqué ? Appuie sur SELECT\net je te montre un coup.") \
    T(MSG_MARK_AI, "J’ai marqué ce qui me semble\nmort. Corrige avec A.") \
    T(MSG_BLACK_WINS_BY, "Noir gagne de") \
    T(MSG_WHITE_WINS_BY, "Blanc gagne de") \
    T(MSG_BLACK_WINS, "Noir gagne.") \
    T(MSG_WHITE_WINS, "Blanc gagne.") \
    T(MSG_BY_RESIGNATION, "par abandon") \
    T(MSG_DRAW, "Partie nulle.") \
    T(MSG_YOU_WIN, "Tu gagnes !") \
    T(MSG_YOU_LOSE, "Tu perds.") \
    T(MSG_POINTS, "points") \
    /* Mission failure reasons (MissionRun::fail_reason_id) */ \
    T(MSG_FAIL_MOVES, "Plus de coups.") \
    T(MSG_FAIL_ESCAPED, "La pierre s’est échappée.") \
    T(MSG_FAIL_CAPTURED, "Ton groupe a été capturé.") \
    T(MSG_FAIL_LADDER, "C’est une échelle, et elle\nne tient pas ici.") \
    T(MSG_FAIL_LIVED, "Le groupe a vécu.") \
    T(MSG_FAIL_DIED, "Ton groupe est mort.") \
    T(MSG_FAIL_INSIDE, "Tu as joué dans le seki.") \
    T(MSG_FAIL_MARGIN, "Pas assez de points.") \
    T(MSG_FAIL_LOST, "Tu as perdu la partie.") \
    T(MSG_FAIL_RESIGNED, "Tu as abandonné.")
