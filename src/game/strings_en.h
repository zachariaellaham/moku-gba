// English UI text. Order must match UI_STRING_LIST in strings_list.h (checked by a
// static_assert in strings.cpp). Limits per category: docs/decisions/strings.md.
#pragma once
// Rows are T(ID, text); same order as the id list.
#define UI_TEXT_EN(T) \
    /* Title screen (mockup 1a) */ \
    T(TITLE_TACTICS, "TACTICS OF GO") \
    T(MENU_CAMPAIGN, "CAMPAIGN") \
    T(MENU_SANDBOX, "SANDBOX") \
    T(MENU_OPTIONS, "OPTIONS") \
    T(MENU_CONTINUE, "CONTINUE") \
    T(MSG_TITLE_FOOTER, "©2026 · SAVE") \
    T(MSG_DEBUG_UNLOCK, "Everything unlocked.") \
    /* Save select */ \
    T(TITLE_SAVE_SELECT, "SELECT FILE") \
    T(HUDL_FILE, "FILE") \
    T(VAL_EMPTY, "EMPTY") \
    T(VAL_NEW_GAME, "NEW GAME") \
    T(MENU_ERASE, "ERASE FILE") \
    T(HUDL_MISSIONS, "MISSIONS") \
    T(HUDL_STARS, "STARS") \
    T(HUDL_TIME, "TIME") \
    T(MSG_ERASE_CONFIRM, "Erase this file?\nAll progress is lost.") \
    T(MENU_YES, "YES") \
    T(MENU_NO, "NO") \
    T(MSG_SAVED, "Progress saved.") \
    T(MSG_SAVE_FAILED, "Could not write to SRAM.") \
    T(NAV_SAVE_SELECT, "A SELECT · B BACK") \
    /* Campaign map (mockup 1b) */ \
    T(HUDL_MISSION, "MISSION") \
    T(CHAP_1, "CH.1 BASICS") \
    T(CHAP_2, "CH.2 CAPTURE") \
    T(CHAP_3, "CH.3 LIFE") \
    T(CHAP_4, "CH.4 SHAPE") \
    T(CHAP_5, "CH.5 THE GAME") \
    T(VAL_LOCKED, "LOCKED") \
    T(MSG_LOCKED, "Clear the mission before\nthis one to open it.") \
    T(HINT_MAP_MORE, "11-20 →") \
    T(NAV_MAP, "A START · B BACK") \
    /* Briefing (mockups 1c / 2d) */ \
    T(HUDL_OBJECTIVE, "OBJECTIVE") \
    T(HUDL_RANK, "RANK") \
    T(WHO_SEN, "MASTER SEN") \
    T(WHO_INDI, "INDI") \
    T(WHO_REX, "REX") \
    T(HINT_ADVANCE, "▼ A") \
    T(NAV_BRIEFING, "A START · B BACK") \
    /* HUD labels (mockups 1d, 1e) */ \
    T(HUDL_TO_PLAY, "TO PLAY") \
    T(HUDL_CAPT, "CAPT") \
    T(HUDL_MOVE, "MOVE") \
    T(HUDL_KOMI, "KOMI") \
    T(HUDL_LIBERTIES, "LIBERTIES:") \
    T(HUDL_INFO, "INFO") \
    T(HUDL_MOVES, "MOVES") \
    T(HUDL_ESTIMATE, "ESTIMATE") \
    T(HUDL_TERRITORY, "TERRITORY") \
    T(HUDL_PRISONERS, "PRISONERS") \
    T(HUDL_AREA, "AREA") \
    T(HUDL_TOTAL, "TOTAL") \
    T(HUDL_RESULT, "RESULT") \
    T(HUDL_DEAD, "DEAD") \
    T(HUDL_CAPTURES, "CAPTURES") \
    T(HUDL_UNDOS, "UNDOS") \
    T(HUDL_HINTS, "HINTS") \
    T(HUDL_UNLOCKED, "UNLOCKED") \
    T(HUDL_LEVEL, "LEVEL") \
    T(HUDL_RECORD, "YOUR RECORD") \
    /* Stone colours (per skin: classic / pup / dino) */ \
    T(VAL_BLACK, "BLACK") \
    T(VAL_WHITE, "WHITE") \
    T(VAL_BLUE, "BLUE") \
    T(VAL_ORANGE, "ORANGE") \
    T(VAL_OBSIDIAN, "OBSIDIAN") \
    T(VAL_AMBER, "AMBER") \
    T(VAL_LTR_BLACK, "B") \
    T(VAL_LTR_WHITE, "W") \
    T(VAL_LTR_BLUE, "B") \
    T(VAL_LTR_ORANGE, "O") \
    T(VAL_LTR_OBSIDIAN, "B") \
    T(VAL_LTR_AMBER, "A") \
    /* In-game button hints (mockup 1d bottom bar) */ \
    T(HINT_A_PLACE, "A PLACE") \
    T(HINT_B_UNDO, "B UNDO") \
    T(HINT_SEL_HINT, "SEL HINT") \
    T(HINT_R_PASS, "R PASS") \
    T(HINT_START_PAUSE, "START PAUSE") \
    T(HINT_A_CONFIRM, "A CONFIRM") \
    T(HINT_B_CANCEL, "B CANCEL") \
    T(HINT_L_R_PANEL, "L R PANEL") \
    T(HINT_A_NEXT, "A ▶") \
    T(HINT_HINTS_SHOWN, "SEL HINT · SHOWN") \
    T(HINT_MARK_DEAD, "A MARK DEAD") \
    T(HINT_MARK_DONE, "START DONE") \
    /* Event banners (mockups 1e, 2b, 2c). CAPTURE gets a runtime suffix: tr(BAN_CAPTURE) + " +N" */ \
    T(BAN_ATARI, "ATARI!") \
    T(BAN_ATARI_PUP, "UH-OH! ATARI") \
    T(BAN_ATARI_DINO, "ROAR! ATARI") \
    T(BAN_CAPTURE, "CAPTURE!") \
    T(BAN_PASS, "PASS") \
    T(BAN_KO, "KO!") \
    T(BAN_RESIGN, "RESIGNED") \
    /* Atari hint line (mockup 1e). Scene appends " " + coordinate + "." */ \
    T(MSG_ATARI_BLACK, "Black has one breath left at") \
    T(MSG_ATARI_WHITE, "White has one breath left at") \
    /* Illegal moves */ \
    T(MSG_ILLEGAL_OCCUPIED, "There is already a stone here.") \
    T(MSG_ILLEGAL_SUICIDE, "A stone with no breath cannot\nbe played there.") \
    T(MSG_ILLEGAL_KO, "Ko: play somewhere else\nfirst.") \
    T(MSG_ILLEGAL_SUPERKO, "That would repeat the whole\nposition.") \
    /* Mission clear / fail (mockup 1f) */ \
    T(TITLE_MISSION_CLEAR, "MISSION CLEAR") \
    T(TITLE_MISSION_FAIL, "MISSION FAILED") \
    T(MENU_RETRY, "RETRY") \
    T(MENU_NEXT, "NEXT MISSION") \
    T(MENU_MAP, "MAP") \
    T(MSG_STARS_EARNED, "Rank A or better, no hints,\nno undos.") \
    /* Unlock messages, in UNLOCK_* order (missions.h): index with MSG_UNLOCK_SANDBOX_9_EASY + id - 1 */ \
    T(MSG_UNLOCK_SANDBOX_9_EASY, "Sandbox · 9×9 vs EASY AI") \
    T(MSG_UNLOCK_NORMAL, "Sandbox · KOAN, NORMAL AI") \
    T(MSG_UNLOCK_13, "Sandbox · 13×13 board") \
    T(MSG_UNLOCK_HARD, "Sandbox · EMBER, HARD AI") \
    T(MSG_UNLOCK_19_MASTER, "Sandbox · 19×19 + TENGEN") \
    /* Sandbox setup (mockup 1g) */ \
    T(TITLE_SANDBOX, "SANDBOX") \
    T(HUDL_BOARD, "BOARD") \
    T(HUDL_YOU_PLAY, "YOU PLAY") \
    T(HUDL_AI, "AI") \
    T(HUDL_HANDICAP, "HANDICAP") \
    T(HUDL_SCORING, "SCORING") \
    T(HUDL_SKIN, "SKIN") \
    T(HUDL_PLAYERS, "PLAYERS") \
    T(MENU_START_GAME, "START GAME") \
    T(VAL_AREA, "AREA") \
    T(VAL_TERRITORY, "TERRITORY") \
    T(VAL_VS_AI, "VS AI") \
    T(VAL_HOTSEAT, "2 PLAYERS") \
    T(VAL_PLAYER_1, "PLAYER 1") \
    T(VAL_PLAYER_2, "PLAYER 2") \
    T(MSG_PASS_DEVICE, "Pass the console to the\nother player.") \
    T(NAV_SANDBOX, "← → CHANGE · A START · B BACK") \
    /* AI levels and their one-line descriptions (mockup 1g right column) */ \
    T(VAL_LVL_VERY_EASY, "VERY EASY") \
    T(VAL_LVL_EASY, "EASY") \
    T(VAL_LVL_NORMAL, "NORMAL") \
    T(VAL_LVL_HARD, "HARD") \
    T(VAL_LVL_MASTER, "MASTER") \
    T(LVLB_VERY_EASY, "Plays at random.\nNever fills its\nown eyes.") \
    T(LVLB_EASY, "Takes every\ncapture. Saves\nits own stones.") \
    T(LVLB_NORMAL, "Reads shapes,\nterritory, one\nmove ahead.") \
    T(LVLB_HARD, "Searches\nthousands of\nfutures. 2 s.") \
    T(LVLB_MASTER, "Full search with\nladders. 5 s of\nthought.") \
    /* Choose opponent (mockup 2a). Character names are proper nouns: identical in both languages. */ \
    T(TITLE_CHOOSE_OPPONENT, "CHOOSE OPPONENT") \
    T(OPP_PEBBLE, "PEBBLE") \
    T(OPP_SPROUT, "SPROUT") \
    T(OPP_KOAN, "KOAN") \
    T(OPP_EMBER, "EMBER") \
    T(OPP_TENGEN, "TENGEN") \
    T(OPPT_PEBBLE, "THE SLEEPY TURTLE") \
    T(OPPT_SPROUT, "THE EAGER SEEDLING") \
    T(OPPT_KOAN, "THE WANDERING MONK") \
    T(OPPT_EMBER, "THE FOX GENERAL") \
    T(OPPT_TENGEN, "THE OLD MASTER") \
    T(BLURB_PEBBLE, "Plays almost at random. Never\nfills its own eyes, though.") \
    T(BLURB_SPROUT, "Grabs every capture it sees\nand saves its stones in atari.") \
    T(BLURB_KOAN, "Reads shapes and territory one\nmove ahead. Solid and patient.") \
    T(BLURB_EMBER, "Searches thousands of futures.\nThinks for two seconds.") \
    T(BLURB_TENGEN, "Full search with ladder\nreading. Five seconds of thought.") \
    T(MSG_BEST_WIN_BY, "BEST: WIN BY") \
    T(MSG_NO_RECORD, "No game played yet.") \
    T(VAL_W, "W") \
    T(VAL_L, "L") \
    T(NAV_OPPONENT, "← → · A CONFIRM") \
    /* Opponent speech bubbles (mockup 2b): intro, on capturing, on being captured, win, lose, atari taunt */ \
    T(SAY_PEBBLE_INTRO, "Oh… is it my turn?") \
    T(SAY_PEBBLE_CAPTURE, "I took one? Really?") \
    T(SAY_PEBBLE_CAPTURED, "Aw. Take it, take it.") \
    T(SAY_PEBBLE_WIN, "I won? Nap time then.") \
    T(SAY_PEBBLE_LOSE, "Good game. Zzz…") \
    T(SAY_PEBBLE_ATARI, "Um… that looks tight.") \
    T(SAY_SPROUT_INTRO, "Let us grow! My turn!") \
    T(SAY_SPROUT_CAPTURE, "Got it! Got it!") \
    T(SAY_SPROUT_CAPTURED, "Hey! That was mine!") \
    T(SAY_SPROUT_WIN, "I win! I win! Again?") \
    T(SAY_SPROUT_LOSE, "Aww. One more game?") \
    T(SAY_SPROUT_ATARI, "One breath left! Eek!") \
    T(SAY_KOAN_INTRO, "The road is long. Sit.") \
    T(SAY_KOAN_CAPTURE, "Back to the bowl.") \
    T(SAY_KOAN_CAPTURED, "Nothing is kept forever.") \
    T(SAY_KOAN_WIN, "The board never lies.") \
    T(SAY_KOAN_LOSE, "You taught me today.") \
    T(SAY_KOAN_ATARI, "One breath is still one.") \
    T(SAY_EMBER_INTRO, "Burn bright, challenger.") \
    T(SAY_EMBER_CAPTURE, "Into the fire they go.") \
    T(SAY_EMBER_CAPTURED, "A scratch. Nothing more.") \
    T(SAY_EMBER_WIN, "Ashes. Come back warmer.") \
    T(SAY_EMBER_LOSE, "Well fought. I’ll burn\nbrighter next time.") \
    T(SAY_EMBER_ATARI, "One breath. Smell that?") \
    T(SAY_TENGEN_INTRO, "Show me the centre.") \
    T(SAY_TENGEN_CAPTURE, "So it goes.") \
    T(SAY_TENGEN_CAPTURED, "Good. You saw it.") \
    T(SAY_TENGEN_WIN, "Play a thousand games.") \
    T(SAY_TENGEN_LOSE, "The old tree bends.") \
    T(SAY_TENGEN_ATARI, "Count your breaths.") \
    /* Options (mockup 2e) */ \
    T(TITLE_OPTIONS, "OPTIONS") \
    T(HUDL_LANGUAGE, "LANGUAGE") \
    T(HUDL_HELPER, "HELPER") \
    T(HUDL_EFFECTS, "EFFECTS") \
    T(HUDL_CONFIRM, "CONFIRM") \
    T(HUDL_MUSIC, "MUSIC") \
    T(HUDL_SFX, "SFX") \
    T(VAL_LANG_EN, "EN") \
    T(VAL_LANG_FR, "FR") \
    T(VAL_SKIN_CLASSIC, "CLASSIC") \
    T(VAL_SKIN_PUP, "PUP") \
    T(VAL_SKIN_DINO, "DINO") \
    T(VAL_EFFECTS_OFF, "OFF") \
    T(VAL_EFFECTS_JUICY, "JUICY") \
    T(VAL_CONFIRM_2A, "2×A") \
    T(VAL_CONFIRM_1A, "1×A") \
    T(MSG_HELPER_ROLE, "Comments on missions and\nevents.") \
    T(MSG_HELPER_SEN, "Master Sen. Calm, precise,\nnever in a hurry.") \
    T(MSG_HELPER_INDI, "Indi the pup. Loud, loyal,\nvery excited.") \
    T(MSG_HELPER_REX, "Rex the little rex. Always\nhungry for stones.") \
    T(NAV_OPTIONS, "← → CHANGE · B BACK") \
    /* Helper interjections (bubble on events; also prefixes for tutor lines) */ \
    T(INTJ_SEN_ATARI, "One breath left.") \
    T(INTJ_SEN_CAPTURE, "Well taken.") \
    T(INTJ_SEN_CLEAR, "Well played.") \
    T(INTJ_SEN_FAIL, "Breathe. Again.") \
    T(INTJ_SEN_THINK, "Take your time.") \
    T(INTJ_INDI_ATARI, "Uh-oh! One breath!") \
    T(INTJ_INDI_CAPTURE, "Good dog! Got them!") \
    T(INTJ_INDI_CLEAR, "WOOF! You did it!") \
    T(INTJ_INDI_FAIL, "Aw… let’s retry!") \
    T(INTJ_INDI_THINK, "Thinking hard!") \
    T(INTJ_REX_ATARI, "One breath. BITE!") \
    T(INTJ_REX_CAPTURE, "CHOMP! Tasty.") \
    T(INTJ_REX_CLEAR, "RAWR! Victory!") \
    T(INTJ_REX_FAIL, "Grr. Try again!") \
    T(INTJ_REX_THINK, "Hmm… rawr?") \
    T(VOICE_INDI, "Woof!") \
    T(VOICE_REX, "Rawr!") \
    /* Pause menu (mockup 1h) */ \
    T(TITLE_PAUSED, "PAUSED") \
    T(MENU_RESUME, "RESUME") \
    T(MENU_UNDO, "UNDO") \
    T(MENU_HINT, "HINT") \
    T(MENU_PASS, "PASS") \
    T(MENU_ESTIMATE, "ESTIMATE SCORE") \
    T(MENU_RESIGN, "RESIGN") \
    T(MENU_QUIT, "QUIT") \
    T(MSG_RESIGN_CONFIRM, "Resign this game?") \
    T(MSG_QUIT_CONFIRM, "Quit the mission?\nThe game will be lost.") \
    T(MSG_ESTIMATE_NOTE, "Estimate only. Dead stones\nare a guess.") \
    T(MSG_NO_HINT, "No hint here. Trust your\nown reading.") \
    T(MSG_NO_UNDO, "Nothing to undo.") \
    T(NAV_PAUSE, "START RESUME · B BACK") \
    /* End of game: dead stone marking, then the score screen */ \
    T(TITLE_GAME_OVER, "GAME OVER") \
    T(TITLE_SCORE, "FINAL SCORE") \
    T(MSG_MARK_DEAD, "Both passed. Mark the dead\nstones, then press START.") \
    T(MSG_STUCK, "Stuck? Press SELECT and\nI will show you a move.") \
    T(MSG_MARK_AI, "I marked what looks dead to\nme. Change it with A.") \
    T(MSG_BLACK_WINS_BY, "Black wins by") \
    T(MSG_WHITE_WINS_BY, "White wins by") \
    T(MSG_BLACK_WINS, "Black wins.") \
    T(MSG_WHITE_WINS, "White wins.") \
    T(MSG_BY_RESIGNATION, "by resignation") \
    T(MSG_DRAW, "A perfect draw.") \
    T(MSG_YOU_WIN, "You win!") \
    T(MSG_YOU_LOSE, "You lose.") \
    T(MSG_POINTS, "points") \
    /* Mission failure reasons (MissionRun::fail_reason_id) */ \
    T(MSG_FAIL_MOVES, "Out of moves.") \
    T(MSG_FAIL_ESCAPED, "The stone escaped.") \
    T(MSG_FAIL_CAPTURED, "Your group was captured.") \
    T(MSG_FAIL_LADDER, "That is a ladder, and this\nladder does not work.") \
    T(MSG_FAIL_LIVED, "The group lived.") \
    T(MSG_FAIL_DIED, "Your group died.") \
    T(MSG_FAIL_INSIDE, "You played inside the seki.") \
    T(MSG_FAIL_MARGIN, "Not enough points.") \
    T(MSG_FAIL_LOST, "You lost the game.") \
    T(MSG_FAIL_RESIGNED, "You resigned.")
