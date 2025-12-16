/** @file parser.c
 * @brief Implementation of command line parsing
 * @author Nom1
 * @author Nom2
 * @date 2025-26
 * @details Implémentation des fonctions d'analyse des lignes de commande.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "parser.h"
#include "processus.h"

/** @brief Fonction de suppression des espaces inutiles au début et à la fin. */
int trim(char* str) {
    if (!str) return -1;
    size_t len = strlen(str);
    if (len == 0) return 0;

    // Suppression à la fin (y compris le \n de fgets)
    char* end = str + len - 1;
    while (end >= str && (isspace(*end) || *end == '\n')) {
        end--;
    }
    *(end + 1) = '\0';

    // Suppression au début
    char* start = str;
    while (*start && isspace(*start)) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    return 0;
}

/** @brief Fonction de nettoyage (suppression doublons d'espaces). */
int clean(char* str) {
    if (!str) return -1;
    char *dest = str;
    char *src = str;
    int space_found = 0;

    while (*src) {
        if (isspace(*src)) {
            if (!space_found) {
                *dest++ = ' ';
                space_found = 1;
            }
        } else {
            *dest++ = *src;
            space_found = 0;
        }
        src++;
    }
    *dest = '\0';
    return trim(str); // On re-trim pour nettoyer un éventuel espace final
}

/** @brief Ajout d'espaces autour des séparateurs. */
int separate_s(char* str, char* s, size_t max) {
    if (!str || !s) return -1;
    
    char buffer[MAX_CMD_LINE];
    size_t i = 0, j = 0;
    
    while (str[i] != '\0' && j < max - 2) {
        int is_sep = 0;
        // Vérifie si le caractère courant est un séparateur
        for (int k = 0; s[k] != '\0'; k++) {
            if (str[i] == s[k]) {
                is_sep = 1;
                break;
            }
        }

        if (is_sep) {
            buffer[j++] = ' ';
            buffer[j++] = str[i];
            buffer[j++] = ' ';
        } else {
            buffer[j++] = str[i];
        }
        i++;
    }
    buffer[j] = '\0';
    
    if (j >= max) return -1; // Dépassement de taille
    
    strcpy(str, buffer);
    return clean(str); // On nettoie les espaces multiples créés
}

/** @brief Remplacement de sous-chaîne. */
int replace(char* str, const char* s, const char* t, size_t max) {
    char buffer[MAX_CMD_LINE];
    char *p;
    
    if (!(p = strstr(str, s))) return 0; // Pas d'occurrence

    // Copie préfixe
    size_t prefix_len = p - str;
    if (prefix_len >= max) return -1;
    strncpy(buffer, str, prefix_len);
    buffer[prefix_len] = '\0';

    // Ajout remplacement + suffixe
    snprintf(buffer + prefix_len, max - prefix_len, "%s%s", t, p + strlen(s));

    if (strlen(buffer) >= max) return -1;
    strcpy(str, buffer);
    
    // Appel récursif pour traiter les occurrences suivantes
    return replace(str, s, t, max);
}

/** @brief Substitution des variables d'environnement ($VAR). */
int substenv(char* str, size_t max) {
    char buffer[MAX_CMD_LINE] = {0};
    char varname[MAX_ENV];
    size_t i = 0, j = 0;
    
    while (str[i] && j < max - 1) {
        if (str[i] == '$') {
            i++; // Passer le $
            int v = 0;
            // Format ${VAR} ou $VAR
            if (str[i] == '{') {
                i++;
                while (str[i] && str[i] != '}') varname[v++] = str[i++];
                if (str[i] == '}') i++;
            } else {
                while (str[i] && (isalnum(str[i]) || str[i] == '_')) varname[v++] = str[i++];
            }
            varname[v] = '\0';
            
            char* val = getenv(varname);
            if (val) {
                size_t len = strlen(val);
                if (j + len >= max) return -1;
                strcpy(buffer + j, val);
                j += len;
            }
            // Si la variable n'existe pas, on ne copie rien (remplacé par vide)
        } else {
            buffer[j++] = str[i++];
        }
    }
    buffer[j] = '\0';
    strcpy(str, buffer);
    return 0;
}

/** @brief Découpage en tokens. */
int strcut(char* str, char sep, char** tokens, size_t max) {
    int count = 0;
    char delim[2] = {sep, '\0'};
    char* ptr = NULL;
    
    // strtok_r est thread-safe et plus robuste que strtok
    char* token = strtok_r(str, delim, &ptr);
    while (token != NULL && count < (int)max - 1) {
        tokens[count++] = token;
        token = strtok_r(NULL, delim, &ptr);
    }
    tokens[count] = NULL;
    return count;
}

/** @brief Analyse de la ligne de commande. */
int parse_command_line(command_line_t* cmdl, const char* line) {
    // 1. Copie et nettoyage initial
    strncpy(cmdl->command_line, line, MAX_CMD_LINE - 1);
    cmdl->command_line[MAX_CMD_LINE - 1] = '\0';

    if (trim(cmdl->command_line) != 0) return -1;
    if (clean(cmdl->command_line) != 0) return -1;

    // 2. Séparation des opérateurs et substitution
    // On sépare ; | & < > !
    if (separate_s(cmdl->command_line, ";|&<>!", MAX_CMD_LINE) != 0) return -1;
    if (substenv(cmdl->command_line, MAX_CMD_LINE) != 0) return -1;
    
    // 3. Tokenisation
    int num_tokens = strcut(cmdl->command_line, ' ', cmdl->tokens, MAX_CMD_LINE / 2 + 1);
    if (num_tokens < 0) return -1;
    if (num_tokens == 0) return 0; // Ligne vide

    // 4. Analyse logique
    int token_index = 0;
    int argv_index = 0;
    processus_t* current_proc = add_processus(cmdl, UNCONDITIONAL);
    if (!current_proc) return -1;

    while (cmdl->tokens[token_index] != NULL) {
        char* token = cmdl->tokens[token_index];
        char* next_token = cmdl->tokens[token_index + 1];
        char* next_next_token = (next_token) ? cmdl->tokens[token_index + 2] : NULL;
        
        int is_operator = 0;

        // --- Opérateurs de Contrôle de flux --- //

        // Cas : ;
        if (strcmp(token, ";") == 0) {
            is_operator = 1;
            // Si ce n'est pas la fin de la ligne, on prépare la suite
            if (next_token) {
                current_proc = add_processus(cmdl, UNCONDITIONAL);
                argv_index = 0;
            }
        }
        // Cas : | ou ||
        else if (strcmp(token, "|") == 0) {
            is_operator = 1;
            if (next_token && strcmp(next_token, "|") == 0) {
                // C'est un OR (||)
                token_index++; // On consomme le 2ème |
                if (cmdl->tokens[token_index + 1]) {
                    current_proc = add_processus(cmdl, ON_FAILURE);
                    argv_index = 0;
                }
            } else {
                // C'est un PIPE (|)
                int pfd[2];
                if (pipe(pfd) == -1) {
                    perror("pipe");
                    close_fds(cmdl);
                    return -1;
                }
                // Redirection sortie du courant -> entrée du tube
                current_proc->stdout_fd = pfd[1];
                add_fd(cmdl, pfd[1]);

                // Création du processus suivant (connecté inconditionnellement)
                current_proc = add_processus(cmdl, UNCONDITIONAL);
                if (!current_proc) return -1;
                argv_index = 0;

                // Redirection entrée du suivant -> sortie du tube
                current_proc->stdin_fd = pfd[0];
                add_fd(cmdl, pfd[0]);
            }
        }
        // Cas : & ou &&
        else if (strcmp(token, "&") == 0) {
            is_operator = 1;
            if (next_token && strcmp(next_token, "&") == 0) {
                // C'est un AND (&&)
                token_index++; // On consomme le 2ème &
                if (cmdl->tokens[token_index + 1]) {
                    current_proc = add_processus(cmdl, ON_SUCCESS);
                    argv_index = 0;
                }
            } else {
                // C'est un Background (&)
                current_proc->is_background = 1;
                // Si une commande suit immédiatement sans séparateur (ex: ls & ls), c'est implicitement un ;
                if (next_token && strcmp(next_token, ";") != 0 && strcmp(next_token, "|") != 0) {
                     // On laisse la boucle traiter, le prochain token sera une nouvelle commande
                     // Mais add_processus n'a pas été appelé. 
                     // Pour simplifier, on assume que & termine la commande courante.
                     if (cmdl->tokens[token_index + 1]) {
                        current_proc = add_processus(cmdl, UNCONDITIONAL);
                        argv_index = 0;
                     }
                }
            }
        }

        // --- Redirections --- //
        
        // Entrée standard (< input)
        else if (strcmp(token, "<") == 0) {
            is_operator = 1;
            token_index++; // On passe le <
            if (!cmdl->tokens[token_index]) { fprintf(stderr, "Erreur syntaxe <\n"); return -1; }
            
            int fd = open(cmdl->tokens[token_index], O_RDONLY);
            if (fd < 0) {
                perror("open input");
                // On peut marquer une erreur de statut sans crasher tout le shell
                current_proc->status = 1; 
            } else {
                current_proc->stdin_fd = fd;
                add_fd(cmdl, fd);
            }
        }
        // Sorties standards (> ou >>)
        else if (strcmp(token, ">") == 0) {
            is_operator = 1;
            int flags = O_WRONLY | O_CREAT | O_TRUNC; // Mode >
            
            // Vérification >> (separate_s a séparé >> en > >)
            if (next_token && strcmp(next_token, ">") == 0) {
                flags = O_WRONLY | O_CREAT | O_APPEND; // Mode >>
                token_index++; // On consomme le 2ème >
            }
            
            token_index++; // On passe au nom du fichier
            if (!cmdl->tokens[token_index]) { fprintf(stderr, "Erreur syntaxe >\n"); return -1; }

            // Gestion simple de >&2 (si l'utilisateur a mis des espaces : > & 2)
            // Pour ce projet, on se concentre sur les noms de fichiers
            int fd = open(cmdl->tokens[token_index], flags, 0644);
            if (fd < 0) {
                perror("open output");
            } else {
                current_proc->stdout_fd = fd;
                add_fd(cmdl, fd);
            }
        }
        // Sortie erreur (2> ou 2>>)
        // Note: separate_s a séparé "2>" en "2" ">". 
        // On détecte donc: Token="2", Next=">"
        else if (strcmp(token, "2") == 0 && next_token && strcmp(next_token, ">") == 0) {
            is_operator = 1;
            token_index++; // On est sur >
            
            int flags = O_WRONLY | O_CREAT | O_TRUNC;
            // Check >>
            if (cmdl->tokens[token_index + 1] && strcmp(cmdl->tokens[token_index + 1], ">") == 0) {
                flags = O_WRONLY | O_CREAT | O_APPEND;
                token_index++; // On est sur le 2ème >
            }
            
            token_index++; // Fichier
            if (!cmdl->tokens[token_index]) return -1;

            // Gestion 2>&1
            if (strcmp(cmdl->tokens[token_index], "&") == 0 && 
                cmdl->tokens[token_index + 1] && 
                strcmp(cmdl->tokens[token_index + 1], "1") == 0) {
                    current_proc->stderr_fd = current_proc->stdout_fd;
                    token_index++; // Passe &
                    token_index++; // Passe 1 (sera incrémenté par la boucle)
                    token_index--; // Ajustement car la boucle fait ++
            } else {
                int fd = open(cmdl->tokens[token_index], flags, 0644);
                if (fd >= 0) {
                    current_proc->stderr_fd = fd;
                    add_fd(cmdl, fd);
                } else {
                    perror("open stderr");
                }
            }
        }

        // --- Modificateurs --- //
        
        // Inversion !
        else if (strcmp(token, "!") == 0) {
            // Uniquement si c'est au début de la commande
            if (argv_index == 0) {
                current_proc->invert = 1;
                is_operator = 1;
            }
        }

        // --- Arguments normaux --- //
        
        if (!is_operator) {
            if (argv_index < MAX_ARGS - 1) {
                if (argv_index == 0) current_proc->path = token;
                current_proc->argv[argv_index++] = token;
                current_proc->argv[argv_index] = NULL;
            } else {
                fprintf(stderr, "Erreur: trop d'arguments (max %d)\n", MAX_ARGS);
                return -1;
            }
        }

        token_index++;
    }

    return 0;
}