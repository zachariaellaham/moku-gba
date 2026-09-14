// French mission text. Order must match MISSION_TEXT_LIST in mission_text_list.h.
#pragma once
// Rows are T(ID, text); same order as the id list.
#define MISSION_TEXT_FR(T) \
    /* Mission 1 — First Stone (7x7): placing stones, turns */ \
    T(MNAME_01, "PREMIER COUP") \
    T(OBJ_01, "Pose une pierre sur les\ntrois marques.") \
    T(RANKH_01, "Trois coups, sans indice.") \
    T(DLG_01_BEFORE, "Bienvenue. Une pierre va\nsur un croisement. Pose-en\nune sur chaque marque.") \
    T(DLG_01_HINT, "Déplace-toi avec la croix,\nappuie sur A pour poser.") \
    T(DLG_01_WIN, "Bien. Le plateau t’attend.") \
    T(DLG_01_FAIL, "Aucun souci. Retourne sur\nles points marqués.") \
    /* Mission 2 — Breath (7x7): liberties */ \
    T(MNAME_02, "SOUFFLE") \
    T(OBJ_02, "Réduis la pierre marquée\nà un seul souffle.") \
    T(RANKH_02, "Deux coups, sans annuler.") \
    T(DLG_02_BEFORE, "Chaque point vide voisin\nest un souffle. Enlève-les\net la pierre tombe.") \
    T(DLG_02_HINT, "Joue à côté de la pierre,\nsur un croisement vide.") \
    T(DLG_02_WIN, "Un seul souffle : on dit\nqu’elle est en atari.") \
    T(DLG_02_FAIL, "Elle a trouvé de l’air.\nCommence par les souffles.") \
    /* Mission 3 — Take It (7x7): capture */ \
    T(MNAME_03, "PRENDS-LA") \
    T(OBJ_03, "Capture la pierre blanche\nmarquée.") \
    T(RANKH_03, "Un coup, sans indice.") \
    T(DLG_03_BEFORE, "Une pierre sans souffle\nest capturée. Celle-ci n’a\nplus qu’un souffle.") \
    T(DLG_03_HINT, "Bouche le dernier souffle.") \
    T(DLG_03_WIN, "Net. La pierre est à toi.") \
    T(DLG_03_FAIL, "Ratée. Vise le dernier\nsouffle.") \
    /* Mission 4 — Run! (7x7): escaping atari */ \
    T(MNAME_04, "SAUVE-TOI !") \
    T(OBJ_04, "Donne trois souffles à ta\npierre marquée.") \
    T(RANKH_04, "Un coup, sans annuler.") \
    T(DLG_04_BEFORE, "Ta pierre est en atari :\nun souffle. Fuis vers le\ncentre, là où c’est vide.") \
    T(DLG_04_HINT, "Étends-toi loin du blanc,\npas vers le bord.") \
    T(DLG_04_WIN, "Trois souffles : elle vit.") \
    T(DLG_04_FAIL, "Attrapée. Fuis du côté le\nplus large.") \
    /* Mission 5 — Connect (9x9): connection vs cut */ \
    T(MNAME_05, "CONNEXION") \
    T(OBJ_05, "Réunis tes deux groupes\nmarqués en un seul.") \
    T(RANKH_05, "Un coup, sans indice.") \
    T(DLG_05_BEFORE, "Deux pierres voisines\npartagent leurs souffles.\nUnies, elles résistent.") \
    T(DLG_05_HINT, "Un seul point les relie.") \
    T(DLG_05_WIN, "Un seul groupe. Bien plus\nsolide.") \
    T(DLG_05_FAIL, "Blanc a coupé. Trouve le\npoint de liaison.") \
    /* Mission 6 — Ladder (9x9): the ladder */ \
    T(MNAME_06, "ÉCHELLE") \
    T(OBJ_06, "Capture la pierre marquée\npar une échelle.") \
    T(RANKH_06, "Cinq coups, sans indice.") \
    T(DLG_06_BEFORE, "Atari, puis encore atari.\nÀ chaque pas un souffle :\nc’est l’échelle.") \
    T(DLG_06_HINT, "Garde-la à un souffle et\npousse-la vers le bord.") \
    T(DLG_06_WIN, "L’échelle tient jusqu’au\nbout. Bien lu.") \
    T(DLG_06_FAIL, "Elle s’échappe. Atari de\nl’autre côté.") \
    /* Mission 7 — Last Breath (9x9): capturing a group (mockups 1c / 2d) */ \
    T(MNAME_07, "DERNIER SOUFFLE") \
    T(OBJ_07, "Capture le groupe blanc\nmarqué.") \
    T(RANKH_07, "En 1 coup, sans annuler.") \
    T(DLG_07_BEFORE, "Deux pierres unies mettent\nleurs souffles en commun.\nIl ne leur en reste qu’un.") \
    T(DLG_07_HINT, "Les deux tombent ensemble.\nUn seul point suffit.") \
    T(DLG_07_WIN, "Deux prisonniers : un\ngroupe meurt d’un bloc.") \
    T(DLG_07_FAIL, "Elles ont repris un\nsouffle. Recompte.") \
    /* Mission 8 — Net (9x9): the net (geta) */ \
    T(MNAME_08, "FILET") \
    T(OBJ_08, "Attrape la pierre marquée\nsans échelle.") \
    T(RANKH_08, "Un coup, sans indice.") \
    T(DLG_08_BEFORE, "Ici l’échelle casse. Ne\ntouche pas la pierre :\nlance un filet.") \
    T(DLG_08_HINT, "Joue à un point d’écart,\nen diagonale.") \
    T(DLG_08_WIN, "Le filet tient. Plus de\nsortie.") \
    T(DLG_08_FAIL, "L’atari la pousse dehors.\nEntoure de loin.") \
    /* Mission 9 — Ko (9x9): the ko rule */ \
    T(MNAME_09, "KO") \
    T(OBJ_09, "Prends le ko, puis\nferme-le pour de bon.") \
    T(RANKH_09, "Deux coups, sans indice.") \
    T(DLG_09_BEFORE, "Ko : blanc ne peut pas\nreprendre tout de suite.\nCe coup libre est à toi.") \
    T(DLG_09_HINT, "Capture, puis bouche le\npoint de reprise.") \
    T(DLG_09_WIN, "Le ko est fermé. Ce coup\nlibre valait tout.") \
    T(DLG_09_FAIL, "Blanc a repris. Ferme le\nko tant que tu peux.") \
    /* Mission 10 — One Eye (9x9): one eye dies */ \
    T(MNAME_10, "UN ŒIL") \
    T(OBJ_10, "Tue le groupe blanc\nmarqué.") \
    T(RANKH_10, "Deux coups, sans indice.") \
    T(DLG_10_BEFORE, "Un œil est un point vide\ndans un groupe. Un seul\nne suffit pas pour vivre.") \
    T(DLG_10_HINT, "Bouche les souffles du\ndehors, puis l’œil.") \
    T(DLG_10_WIN, "Un œil, c’est une maison\nsans porte. Il meurt.") \
    T(DLG_10_FAIL, "Il a fait un second œil.\nRecommence.") \
    /* Mission 11 — Two Eyes (9x9): life */ \
    T(MNAME_11, "DEUX YEUX") \
    T(OBJ_11, "Donne deux yeux au\ngroupe marqué.") \
    T(RANKH_11, "Un coup, sans indice.") \
    T(DLG_11_BEFORE, "Deux yeux séparés : le\ngroupe vit à jamais. Blanc\nne peut pas les boucher.") \
    T(DLG_11_HINT, "Coupe le grand espace vide\nen deux.") \
    T(DLG_11_WIN, "Vivant. Plus rien ne peut\nl’atteindre.") \
    T(DLG_11_FAIL, "Un seul œil, ou un faux.\nRéessaie.") \
    /* Mission 12 — False Eye (9x9): false eyes */ \
    T(MNAME_12, "FAUX ŒIL") \
    T(OBJ_12, "Tue le groupe au faux œil.") \
    T(RANKH_12, "Deux coups, sans indice.") \
    T(DLG_12_BEFORE, "Cet œil est faux : les\npierres du coin ne sont\npas reliées. Prouve-le.") \
    T(DLG_12_HINT, "Capture la pierre qui\ntient le coin.") \
    T(DLG_12_WIN, "Un faux œil n’est pas un\nœil.") \
    T(DLG_12_FAIL, "Tu as bouché le vrai œil.\nAttaque le coin.") \
    /* Mission 13 — Snapback (9x9): snapback */ \
    T(MNAME_13, "COUP DE RAPPEL") \
    T(OBJ_13, "Capture deux pierres ou plus en coup de rappel.") \
    T(RANKH_13, "Deux coups, sans annuler.") \
    T(DLG_13_BEFORE, "Offre une pierre. Quand\nblanc la mange, tu prends\ntout le groupe.") \
    T(DLG_13_HINT, "Joue dedans, dans l’espace\nde deux points.") \
    T(DLG_13_WIN, "Coup de rappel !\nUne pierre donnée,\ntout le groupe repris.") \
    T(DLG_13_FAIL, "Rien n’est revenu. Le\nsacrifice se joue dedans.") \
    /* Mission 14 — Seki (9x9): seki */ \
    T(MNAME_14, "SEKI") \
    T(OBJ_14, "Atteins le seki et n’y touche plus six coups.") \
    T(RANKH_14, "Une pierre, puis n’y\ntouche plus.") \
    T(DLG_14_BEFORE, "Seki : aucun des deux ne\npeut boucher les souffles\ncommuns. Les deux vivent.") \
    T(DLG_14_HINT, "Ne joue pas dedans. Passe\nou joue au loin.") \
    T(DLG_14_WIN, "Seki. Une trêve qui vaut\nla vie.") \
    T(DLG_14_FAIL, "Boucher un souffle commun\ntue ton propre groupe.") \
    /* Mission 15 — Count (9x9 game vs EASY): scoring. EMBER shows up from here on. */ \
    T(MNAME_15, "COMPTER") \
    T(OBJ_15, "Gagne cette partie 9×9 de\ncinq points ou plus.") \
    T(RANKH_15, "Gagne de cinq : un S.") \
    T(DLG_15_BEFORE, "Une vraie partie. Compte\nen jouant : ta zone, tes\nprisonniers, komi déduit.") \
    T(DLG_15_HINT, "Prends d’abord le grand\ncoin vide.") \
    T(DLG_15_WIN, "Compté et gagné. Les\nchiffres ne mentent pas.") \
    T(DLG_15_FAIL, "Presque. Ferme tes\nfrontières avant la fin.") \
    /* Mission 16 — Cut (13x13): cutting */ \
    T(MNAME_16, "COUPE") \
    T(OBJ_16, "Coupe le blanc et\nprends le groupe coupé.") \
    T(RANKH_16, "Quatre coups, sans indice.") \
    T(DLG_16_BEFORE, "Deux pierres en diagonale\npeuvent se couper. Coupe\nd’abord, réfléchis après.") \
    T(DLG_16_HINT, "Joue le point de coupe\nentre les deux.") \
    T(DLG_16_WIN, "Coupé, puis capturé. C’est\nl’ordre.") \
    T(DLG_16_FAIL, "Blanc s’est relié. La\ncoupe était l’occasion.") \
    /* Mission 17 — Invade 3-3 (13x13): the 3-3 invasion */ \
    T(MNAME_17, "INVASION 3-3") \
    T(OBJ_17, "Vis dans le coin marqué.") \
    T(RANKH_17, "Vis en six coups.") \
    T(DLG_17_BEFORE, "Le point 3-3 est la clé du\ncoin. Plonge et fais deux\nyeux.") \
    T(DLG_17_HINT, "Glisse sous ses pierres,\npuis partage l’espace.") \
    T(DLG_17_WIN, "Vivant dans le coin. Son\nmur ne suffit pas.") \
    T(DLG_17_FAIL, "Plus de place. Descends et\nprends le bord.") \
    /* Mission 18 — Life & Death (13x13): five chained tsumego */ \
    T(MNAME_18, "VIE ET MORT") \
    T(OBJ_18, "Résous cinq problèmes\nd’affilée.") \
    T(RANKH_18, "Sans indice, sans annuler.") \
    T(DLG_18_BEFORE, "Cinq problèmes : prendre,\nvivre, tuer, piéger, lire\njusqu’au bout.") \
    T(DLG_18_HINT, "Cherche le point vital de\nla forme.") \
    T(DLG_18_WIN, "Cinq sur cinq. Ton œil est\naffûté.") \
    T(DLG_18_FAIL, "On reprend. Lis un coup\nplus loin.") \
    /* Mission 19 — Whole Board (13x13 game vs NORMAL) */ \
    T(MNAME_19, "TOUT LE PLATEAU") \
    T(OBJ_19, "Bats KOAN sur tout le\nplateau.") \
    T(RANKH_19, "Bats KOAN : c’est un S.") \
    T(DLG_19_BEFORE, "Les coins, puis les bords,\npuis le centre. Joue là où\nc’est le plus grand.") \
    T(DLG_19_HINT, "Réponds à son approche,\ngarde tes groupes reliés.") \
    T(DLG_19_WIN, "Une vraie partie, bien\njouée. KOAN s’incline.") \
    T(DLG_19_FAIL, "Il a pris les grands\npoints. Prends-les avant.") \
    /* Mission 20 — Tengen (19x19 vs HARD, 3 handicap stones) */ \
    T(MNAME_20, "TENGEN") \
    T(OBJ_20, "Bats EMBER en 19×19\navec trois pierres.") \
    T(RANKH_20, "Bats EMBER : c’est un S.") \
    T(DLG_20_BEFORE, "Tengen, c’est le centre du\nplateau. Tiens tout. C’est\nla dernière mission.") \
    T(DLG_20_HINT, "Sers-toi de tes trois\npierres : joue simple.") \
    T(DLG_20_WIN, "Tu tiens le centre et la\npartie. Bonne route.") \
    T(DLG_20_FAIL, "EMBER brûle fort. Souffle,\npuis recommence.") \
    /* Mission 18 stages — five chained tsumego (MissionDef::stages) */ \
    T(MNAME_T18_1, "PRENDS-EN UNE") \
    T(OBJ_T18_1, "Capture la pierre blanche\nmarquée.") \
    T(DLG_T18_1_BEFORE, "Pour commencer : une\npierre blanche, un souffle\net c’est fini.") \
    T(DLG_T18_1_HINT, "Prends le dernier souffle.") \
    T(DLG_T18_1_WIN, "Prise. Maintenant, du\nsérieux.") \
    T(MNAME_T18_2, "LE MILIEU") \
    T(OBJ_T18_2, "Fais vivre le groupe\nmarqué.") \
    T(DLG_T18_2_BEFORE, "Trois points vides en\nligne. Un seul donne deux\nyeux.") \
    T(DLG_T18_2_HINT, "Prends le point central.") \
    T(DLG_T18_2_WIN, "Deux yeux. Vivant.") \
    T(MNAME_T18_3, "DEUX ESPACES") \
    T(OBJ_T18_3, "Tue le groupe blanc\nmarqué.") \
    T(DLG_T18_3_BEFORE, "Blanc a deux espaces, mais\nun seul est un vrai œil.\nBouche l’autre d’abord.") \
    T(DLG_T18_3_HINT, "L’espace que tes pierres\ntouchent n’est pas un œil.") \
    T(DLG_T18_3_WIN, "Un seul œil. Mort.") \
    T(MNAME_T18_4, "DONNE-EN UNE") \
    T(OBJ_T18_4, "Capture en coup de rappel.") \
    T(DLG_T18_4_BEFORE, "Encore le coin. Offre une\npierre, laisse blanc la\nprendre, puis prends tout.") \
    T(DLG_T18_4_HINT, "Joue dans le coin même.") \
    T(DLG_T18_4_WIN, "Coup de rappel dans le\ncoin. Bien vu.") \
    T(MNAME_T18_5, "JUSQU’AU BORD") \
    T(OBJ_T18_5, "Capture la pierre marquée\npar une échelle.") \
    T(DLG_T18_5_BEFORE, "La dernière. Pousse-la\njusqu’au bord, un souffle\nà la fois.") \
    T(DLG_T18_5_HINT, "Atari du côté libre, puis\ncontinue.") \
    T(DLG_T18_5_WIN, "L’échelle tient. Les cinq\nsont résolus.") \
    T(DLG_T18_FAIL, "Pas cette fois. On remet\nles pierres en place.") \
    /* EMBER the rival, one line per mission from 15 on (shown after the tutor in the briefing) */ \
    T(DLG_RIVAL_M15, "Alors tu sais compter.\nUn jour, tu compteras\ncontre moi.") \
    T(DLG_RIVAL_M16, "La coupe est mon coup\npréféré. Ne me déçois pas,\npetite pierre.") \
    T(DLG_RIVAL_M17, "Tu envahis mon coin ?\nAudacieux. J’aime ça.") \
    T(DLG_RIVAL_M18, "Cinq problèmes. Je les ai\nfaits avant mon premier\nfeu.") \
    T(DLG_RIVAL_M19, "KOAN est patient. Pas moi.\nDépêche-toi de gagner.") \
    T(DLG_RIVAL_M20, "Trois pierres d’avance et\ntout le plateau. Plus\nd’excuses, rival.")
