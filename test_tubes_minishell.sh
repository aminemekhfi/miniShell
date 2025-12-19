#!/bin/bash

SHELL_BIN=./minishell
OUT=RESULTATS_TEST_TUBES.txt

rm -f "$OUT"

echo "========================================" >> "$OUT"
echo "   TEST DES TUBES (PIPES) - MINISHELL   " >> "$OUT"
echo "========================================" >> "$OUT"
echo "" >> "$OUT"

run () {
    echo ">>> $1" >> "$OUT"
    printf "%b\n" "$2" | $SHELL_BIN >> "$OUT" 2>&1
    echo "----------------------------------------" >> "$OUT"
}

# ==================================================
# 1. PIPE SIMPLE
# ==================================================
run "Pipe simple (ls | wc -l)" "ls | wc -l"

# ==================================================
# 2. PIPE AVEC COMMANDE PRODUCTRICE
# ==================================================
run "echo | wc" "echo hello | wc -c"

# ==================================================
# 3. PIPE AVEC grep
# ==================================================
run "grep via pipe" "ls | grep Makefile"

# ==================================================
# 4. PIPE MULTIPLE
# ==================================================
run "Pipe multiple (ls | sort | wc -l)" "ls | sort | wc -l"

# ==================================================
# 5. PIPE + REDIRECTION SORTIE
# ==================================================
run "Pipe + redirection >" $'echo hello | wc -c > count.txt\ncat count.txt'

# ==================================================
# 6. REDIRECTION ENTREE + PIPE
# ==================================================
run "Redirection < + pipe" $'echo test > in.txt\ncat < in.txt | wc -c'

# ==================================================
# 7. STDERR DANS PIPE (2>&1)
# ==================================================
run "stderr dans pipe (2>&1)" "ls fichier_inexistant 2>&1 | wc -l"

# ==================================================
# 8. STDOUT VERS STDERR AVANT PIPE (>&2)
# ==================================================
run "stdout vers stderr puis pipe" "echo hello >&2 | wc -c"

# ==================================================
# 9. PIPE + BUILTIN
# ==================================================
run "builtin dans pipe (pwd | wc -c)" "pwd | wc -c"

# ==================================================
# 10. PIPE EN DEBUT (ERREUR)
# ==================================================
run "Pipe en début (erreur)" "| ls"

# ==================================================
# 11. PIPE EN FIN (ERREUR)
# ==================================================
run "Pipe en fin (erreur)" "ls |"

# ==================================================
# 12. PIPE + LOGIQUE
# ==================================================
run "Pipe + &&" "ls | wc -l && echo OK"
run "Pipe + ||" "ls fichier_inexistant | wc -l || echo FAIL"

# ==================================================
# FIN
# ==================================================
echo "" >> "$OUT"
echo "===== FIN DES TESTS TUBES =====" >> "$OUT"

echo "✅ Tests des tubes terminés."
echo "📄 Résultats dans : $OUT"
