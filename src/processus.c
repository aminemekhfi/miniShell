/** @file processus.c
 * @brief Implementation of process management functions
 * @author Nom1
 * @author Nom2
 * @date 2025-26
 * @details Implémentation des fonctions de gestion des processus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#include "processus.h"
#include "builtins.h"

int last_status = 0;


/**
 * @brief Fonction d'initialisation d'une structure de processus.
 */
int init_processus(processus_t* proc) {
    if (!proc) return -1;
    // On utilise memset pour tout mettre à 0 (NULL pour les pointeurs, 0 pour les entiers)
    memset(proc, 0, sizeof(processus_t));
    
    // Initialisation des descripteurs standards par défaut
    proc->stdin_fd = STDIN_FILENO;
    proc->stdout_fd = STDOUT_FILENO;
    proc->stderr_fd = STDERR_FILENO;
    
    return 0;
}

/** * @brief Fonction de lancement d'un processus à partir d'une structure de processus.
 */
int launch_processus(processus_t* proc) {
    if (!proc) return -1;

    // 1. Gestion des commandes intégrées (Builtins)
    if (is_builtin(proc)) {
        // Sauvegarde des descripteurs standards actuels du shell père
        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);
        int saved_stderr = dup(STDERR_FILENO);

        // Application des redirections si nécessaire
        if (proc->stdin_fd != STDIN_FILENO) dup2(proc->stdin_fd, STDIN_FILENO);
        if (proc->stdout_fd != STDOUT_FILENO) dup2(proc->stdout_fd, STDOUT_FILENO);
        if (proc->stderr_fd != STDERR_FILENO) dup2(proc->stderr_fd, STDERR_FILENO);

        // Exécution de la commande
        int ret = exec_builtin(proc);
        
        // Mise à jour du statut
        proc->status = ret;
        if (proc->invert) proc->status = !proc->status;

        // Restauration des descripteurs standards
        dup2(saved_stdin, STDIN_FILENO); close(saved_stdin);
        dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout);
        dup2(saved_stderr, STDERR_FILENO); close(saved_stderr);

        // Fermeture des fichiers ouverts par cette commande (ex: fichier de redirection)
        // Note : On ferme dans le père car pas de fork pour les builtins
        if (proc->stdin_fd != STDIN_FILENO) close(proc->stdin_fd);
        if (proc->stdout_fd != STDOUT_FILENO) close(proc->stdout_fd);
        if (proc->stderr_fd != STDERR_FILENO) close(proc->stderr_fd);

        return 0;
    }

    // 2. Gestion des commandes externes
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // --- PROCESSUS FILS ---
        
        // Application des redirections
        if (proc->stdin_fd != STDIN_FILENO) {
            if (dup2(proc->stdin_fd, STDIN_FILENO) == -1) { perror("dup2 stdin"); exit(1); }
        }
        if (proc->stdout_fd != STDOUT_FILENO) {
            if (dup2(proc->stdout_fd, STDOUT_FILENO) == -1) { perror("dup2 stdout"); exit(1); }
        }
        /* --- stderr --- */
        if (proc->stderr_fd == -1) {
            // Cas 2>&1 : stderr doit suivre stdout (déjà redirigé ou pipe)
            if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
                perror("dup2 stderr->stdout");
                exit(1);
            }
        }
        else if (proc->stderr_fd != STDERR_FILENO) {
            if (dup2(proc->stderr_fd, STDERR_FILENO) == -1) {
                perror("dup2 stderr");
                exit(1);
            }
        }


        // Fermeture de tous les descripteurs gérés par le shell (pipes, fichiers ouverts)
        // C'est CRUCIAL pour que les pipes fonctionnent (EOF détecté quand tous les écriveurs ferment)
        if (proc->cf && proc->cf->cmdl) {
            close_fds(proc->cf->cmdl);
        }

        // Exécution
        execvp(proc->argv[0], proc->argv);
        
        // Si on arrive ici, c'est une erreur
        fprintf(stderr, "%s: commande introuvable\n", proc->argv[0]);
        exit(127); // Code standard pour command not found
    } 
    else {
        // --- PROCESSUS PÈRE ---
        proc->pid = pid;

        // Fermeture des descripteurs côté père qui ont été passés au fils.
        // C'est indispensable pour les pipes : si le père garde le bout d'écriture ouvert,
        // le lecteur ne recevra jamais EOF.
        if (proc->stdin_fd != STDIN_FILENO) close(proc->stdin_fd);
        if (proc->stdout_fd != STDOUT_FILENO) close(proc->stdout_fd);
        if (proc->stderr_fd != STDERR_FILENO) close(proc->stderr_fd);

        // Gestion de l'attente
        if (!proc->is_background) {
            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                perror("waitpid");
                proc->status = 1;
            } else {
                if (WIFEXITED(wstatus)) {
                    proc->status = WEXITSTATUS(wstatus);
                } else {
                    proc->status = 1; // Terminé par signal ou autre erreur
                }
            }
        } else {
            // En background, on affiche le PID et on considère succès immédiat pour le flux
            printf("[%d] %d\n", 1, pid); // Id job simulé à 1
            proc->status = 0;
        }

        // Gestion de l'inversion (!)
        if (proc->invert) {
            proc->status = !proc->status;
        }
    }

    return 0;
}

/** * @brief Fonction d'initialisation d'une structure de contrôle de flux.
 */
int init_control_flow(control_flow_t* cf) {
    if (!cf) return -1;
    memset(cf, 0, sizeof(control_flow_t));
    return 0;
}

/** * @brief Fonction d'ajout d'un processus à la structure de contrôle de flux.
 */
processus_t* add_processus(command_line_t* cmdl, control_flow_mode_t mode) {
    if (!cmdl) return NULL;
    if (cmdl->num_commands >= MAX_CMDS) {
        fprintf(stderr, "Erreur: Trop de commandes.\n");
        return NULL;
    }

    // Récupération des pointeurs vers les nouvelles structures
    processus_t* new_proc = &cmdl->commands[cmdl->num_commands];
    control_flow_t* new_flow = &cmdl->flow[cmdl->num_commands];

    // Initialisation
    init_processus(new_proc);
    init_control_flow(new_flow);

    // Liaison bidirectionnelle
    new_flow->proc = new_proc;
    new_flow->cmdl = cmdl;
    new_proc->cf = new_flow;

    // Chaînage avec le processus précédent (s'il existe)
    if (cmdl->num_commands > 0) {
        control_flow_t* prev_flow = &cmdl->flow[cmdl->num_commands - 1];
        
        switch (mode) {
            case UNCONDITIONAL:
                prev_flow->unconditionnal_next = new_flow;
                break;
            case ON_SUCCESS:
                prev_flow->on_success_next = new_flow;
                break;
            case ON_FAILURE:
                prev_flow->on_failure_next = new_flow;
                break;
        }
    }

    cmdl->num_commands++;
    return new_proc;
}

/** * @brief Fonction de récupération du prochain processus à exécuter selon le contrôle de flux.
 */
processus_t* next_processus(command_line_t* cmdl) {
    if (!cmdl) return NULL;
    if (cmdl->num_commands >= MAX_CMDS) return NULL;
    
    // Retourne un pointeur vers l'espace mémoire qui SERA utilisé par le prochain add_processus
    // Cela permet au parser de configurer les pipes avant même que le processus ne soit "ajouté" au flux.
    return &cmdl->commands[cmdl->num_commands];
}

/** * @brief Fonction d'ajout d'un descripteur de fichier à la structure de contrôle de flux.
 */
int add_fd(command_line_t* cmdl, int fd) {
    if (!cmdl || fd < 0) return -1;
    
    // Taille du tableau opened_descriptors (défini dans processus.h comme MAX_CMDS * 3 + 1)
    int max_fds = MAX_CMDS * 3 + 1;

    for (int i = 0; i < max_fds; i++) {
        if (cmdl->opened_descriptors[i] == -1) {
            cmdl->opened_descriptors[i] = fd;
            return 0;
        }
    }
    return -1; // Tableau plein
}

/** * @brief Fonction de fermeture des descripteurs de fichiers listés.
 */
int close_fds(command_line_t* cmdl) {
    if (!cmdl) return -1;
    
    int max_fds = MAX_CMDS * 3 + 1;

    for (int i = 0; i < max_fds; i++) {
        if (cmdl->opened_descriptors[i] != -1) {
            close(cmdl->opened_descriptors[i]);
            cmdl->opened_descriptors[i] = -1;
        }
    }
    return 0;
}

/** * @brief Fonction d'initialisation d'une structure de ligne de commande.
 */
int init_command_line(command_line_t* cmdl) {
    if (!cmdl) return -1;
    
    // Mise à zéro complète
    memset(cmdl, 0, sizeof(command_line_t));
    
    // Initialisation spécifique du tableau des descripteurs à -1
    int max_fds = MAX_CMDS * 3 + 1;
    for (int i = 0; i < max_fds; i++) {
        cmdl->opened_descriptors[i] = -1;
    }
    
    return 0;
}

/** * @brief Fonction de lancement d'une ligne de commande.
 */
int launch_command_line(command_line_t* cmdl) {
    if (!cmdl || cmdl->num_commands == 0) return 0;

    // On commence par le premier élément du flux
    control_flow_t* current = &cmdl->flow[0];

    while (current != NULL) {
        processus_t* proc = current->proc;

        if (launch_processus(proc) != 0) {
            fprintf(stderr, "Erreur au lancement du processus\n");
            break;
        }

        last_status = proc->status;

        if (proc->status == 0) {
            if (current->on_success_next != NULL) {
                current = current->on_success_next;
            } else {
                current = current->unconditionnal_next;
            }
        } else {
            if (current->on_failure_next != NULL) {
                current = current->on_failure_next;
            } else {
                current = current->unconditionnal_next;
            }
        }
    }


    // Nettoyage final : on s'assure que tous les descripteurs ouverts (pipes, redirections) sont fermés
    close_fds(cmdl);
    
    return 0;
}