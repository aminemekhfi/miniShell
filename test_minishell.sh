#!/bin/bash

SHELL_BIN=./minishell
OUT=RESULTATS_TEST_MINISHELL.txt

rm -f "$OUT"

echo "========================================" >> "$OUT"
echo "  TEST COMPLET DU PROJET MINISHELL (PSI)" >> "$OUT"
echo "========================================" >> "$OUT"
echo "" >> "$OUT"

run () {
    echo ">>> $1" >> "$OUT"
    printf "%b\n" "$2" | $SHELL_BIN >> "$OUT" 2>&1
    echo "----------------------------------------" >> "$OUT"
}

# ==================================================
# 1. COMMANDES SIMPLES + ARGUMENTS
# ==================================================
run "Commande simple" "ls"
run "Commande avec arguments" "echo hello world"
run "Commande inconnue" "commande_inconnue"

# ==================================================
# 2. FOREGROUND / BACKGROUND
# ==================================================
run "Foreground" "sleep 1; echo done"
run "Background (&)" $'sleep 1 &\necho bg_ok'

# ==================================================
# 3. INVERSION (!)
# ==================================================
run "! true" "! true; echo $?"
run "! false" "! false; echo $?"

# ==================================================
# 4. SEQUENCES INCONDITIONNELLES (;)
# ==================================================
run "Sequence ;" "echo A; echo B; echo C"

# ==================================================
# 5. CONDITIONNELLES && ||
# ==================================================
run "&& succès" "true && echo OK"
run "&& échec" "false && echo FAIL"
run "|| succès" "false || echo OK"
run "|| échec" "true || echo FAIL"

# ==================================================
# 6. COMBINAISONS COMPLEXES
# ==================================================
run "Combinaison logique" "true && ! false || echo SHOULD_NOT_PRINT"
run "Combinaison logique 2" "false || true && echo OK"

# ==================================================
# 7. REDIRECTIONS
# ==================================================
run "Redirection stdin <" $'echo input > in.txt\ncat < in.txt'
run "Redirection stdout >" $'echo hello > out.txt\ncat out.txt'
run "Redirection stdout >>" $'echo world >> out.txt\ncat out.txt'
run "Redirection stderr 2>" $'ls fichier_inexistant 2> err.txt\ncat err.txt'
run "Redirection stderr 2>>" $'ls fichier_inexistant 2>> err.txt\ncat err.txt'

# ==================================================
# 8. REDIRECTIONS AVANCEES
# ==================================================
run "stdout vers stderr >&2" "echo hi >&2"
run "stderr vers stdout 2>&1" "ls fichier_inexistant 2>&1"

# ==================================================
# 9. PIPES
# ==================================================
run "Pipe simple" "ls | wc -l"
run "Pipe multiple" "ls | sort | wc -l"
run "Pipe + grep" "echo hello | grep hell"

# ==================================================
# 10. REDIRECTIONS + PIPES
# ==================================================
run "Pipe + redirection" $'echo hello | wc -c > count.txt\ncat count.txt'
run "2>&1 + pipe" "ls fichier_inexistant 2>&1 | wc -l"
run ">&2 + pipe" "echo hi >&2 | wc -c"

# ==================================================
# 11. BUILTINS
# ==================================================
run "pwd builtin" "pwd"
run "cd /" $'cd /\npwd'
run "cd ~" $'cd ~\npwd'
run "cd erreur" "cd dossier_inexistant"
run "export / unset" $'export TEST=42\necho $TEST\nunset TEST\necho $TEST'

# ==================================================
# 12. VARIABLES ET SUBSTITUTIONS
# ==================================================
run "Variable d'environnement" "echo $HOME"
run "Substitution $?" $'true\necho $?\nfalse\necho $?'

# ==================================================
# 13. SYNTAX ERRORS
# ==================================================
run "Erreur |" "ls |"
run "Erreur >" "ls >"
run "Erreur <" "ls <"

# ==================================================
# 14. EXIT
# ==================================================
echo ">>> exit 42 (test du code de sortie)" >> "$OUT"
printf "exit 42\n" | $SHELL_BIN
echo "Code de sortie minishell (bash): $?" >> "$OUT"
echo "----------------------------------------" >> "$OUT"

# ==================================================
# FIN
# ==================================================
echo "" >> "$OUT"
echo "===== FIN DES TESTS =====" >> "$OUT"

echo "✅ Tests terminés."
echo "📄 Résultats dans : $OUT"
