/** @file builtins.c
 * @brief Implementation of built-in commands
 * @details Implémentation des fonctions des commandes intégrées.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h> // Pour PATH_MAX
#include <errno.h>

#include "builtins.h"
#include "processus.h"

// Déclaration nécessaire pour parcourir l'environnement (pour export sans args)
extern char **environ;

/** @brief Fonction de vérification si une commande est une commande "built-in".
 * @param cmd Nom de la commande à vérifier.
 * @return int 1 si la commande est intégrée, 0 sinon.
 */
int is_builtin(const processus_t* cmd) {
    if (cmd == NULL || cmd->argv[0] == NULL) {
        return 0;
    }
    const char* name = cmd->argv[0];

    if (strcmp(name, "cd") == 0) return 1;
    if (strcmp(name, "exit") == 0) return 1;
    if (strcmp(name, "export") == 0) return 1;
    if (strcmp(name, "unset") == 0) return 1;
    if (strcmp(name, "pwd") == 0) return 1;

    return 0;
}

/** @brief Fonction d'exécution d'une commande intégrée.
 * @param cmd Pointeur vers la structure de commande à exécuter.
 * @return int 0 en cas de succès, -1 en cas d'erreur.
 */
int exec_builtin(processus_t* cmd) {
    if (cmd == NULL || cmd->argv[0] == NULL) {
        return -1;
    }
    const char* name = cmd->argv[0];

    if (strcmp(name, "cd") == 0) return builtin_cd(cmd);
    if (strcmp(name, "exit") == 0) return builtin_exit(cmd);
    if (strcmp(name, "export") == 0) return builtin_export(cmd);
    if (strcmp(name, "unset") == 0) return builtin_unset(cmd);
    if (strcmp(name, "pwd") == 0) return builtin_pwd(cmd);

    return -1; // Commande non trouvée
}

/** Fonctions spécifiques aux commandes intégrées. */

/** @brief Fonction d'exécution de la commande "cd".
 */
int builtin_cd(processus_t* cmd) {
    const char* path = cmd->argv[1];

    // Si pas d'argument, on va vers HOME
    if (path == NULL) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: variable HOME non définie\n");
            return 1;
        }
    }

    // Tentative de changement de répertoire
    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    // Mise à jour de la variable PWD (bonne pratique)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
    }

    return 0;
}

/** @brief Fonction d'exécution de la commande "exit".
 */
int builtin_exit(processus_t *proc)
{
    int status = 0;

    // Si un argument est fourni : exit val
    if (proc->argv[1] != NULL) {
        status = atoi(proc->argv[1]);
    }

    // Quitte le shell avec le code voulu
    exit(status);
}


/** @brief Fonction d'exécution de la commande "export".
 */
int builtin_export(processus_t* cmd) {
    // Si pas d'argument, on affiche l'environnement
    if (cmd->argv[1] == NULL) {
        for (char **env = environ; *env != 0; env++) {
            printf("%s\n", *env);
        }
        return 0;
    }

    char* arg = cmd->argv[1];
    char* equal_sign = strchr(arg, '=');

    if (equal_sign != NULL) {
        // Format VAR=VAL
        *equal_sign = '\0'; // On coupe la chaîne en deux
        char* name = arg;
        char* value = equal_sign + 1;

        // setenv(nom, valeur, overwrite=1)
        if (setenv(name, value, 1) != 0) {
            perror("export");
            return 1;
        }
    } else {
        // Format VAR (sans valeur)
        // Dans POSIX, cela marque la variable pour l'export, 
        // mais ici on peut juste ignorer ou vérifier si elle existe.
        // Simplification: on ne fait rien si pas de valeur fournie.
    }

    return 0;
}

/** @brief Fonction d'exécution de la commande "unset".
 */
int builtin_unset(processus_t* cmd) {
    if (cmd->argv[1] == NULL) {
        fprintf(stderr, "unset: arguments insuffisants\n");
        return 1;
    }

    if (unsetenv(cmd->argv[1]) != 0) {
        perror("unset");
        return 1;
    }

    return 0;
}

/** @brief Fonction d'exécution de la commande "pwd".
 */
int builtin_pwd(processus_t* cmd) {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // On utilise printf qui écrit sur stdout.
        // Comme exec_builtin est appelé APRES les redirections dup2 dans processus.c,
        // ce printf ira bien dans le fichier si redirection il y a.
        printf("%s\n", cwd);
        return 0;
    } else {
        perror("pwd");
        return 1;
    }
}