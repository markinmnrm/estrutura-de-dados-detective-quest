/*
 Detective Quest - Sistema de coleta de pistas, associação a suspeitos e julgamento final
 Autor: (você pode colocar seu nome)
 Compilar: gcc -std=c11 -Wall -Wextra -o detective detective.c
 Executar: ./detective
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------
   Tipos de dados principais
   ---------------------------*/

/* Nó da árvore binária de cômodos (mansão) */
typedef struct Sala {
    char *nome;
    struct Sala *esq;
    struct Sala *dir;
} Sala;

/* Nó da BST que armazena pistas coletadas (ordenada por string) */
typedef struct PistaNode {
    char *pista;
    struct PistaNode *esq;
    struct PistaNode *dir;
} PistaNode;

/* Elemento (entry) da tabela hash (cadeia / encadeamento separado) */
typedef struct HashEntry {
    char *chave;      // pista
    char *valor;      // suspeito
    struct HashEntry *prox;
} HashEntry;

/* Tabela hash simples */
typedef struct HashTable {
    size_t tamanho;   // número de buckets
    HashEntry **buckets;
} HashTable;

/* ---------------------------
   Assinaturas de funções
   ---------------------------*/

/* criarSala() – cria dinamicamente um cômodo. */
Sala *criarSala(const char *nome);

/* explorarSalas() – navega pela árvore e ativa o sistema de pistas. */
void explorarSalas(Sala *raiz, PistaNode **pistasColetadas, HashTable *hash);

/* inserirPista() / adicionarPista() – insere a pista coletada na árvore de pistas. */
PistaNode *inserirPista(PistaNode *raiz, const char *pista);
int existePista(PistaNode *raiz, const char *pista);
void listarPistasInorder(PistaNode *raiz);

/* inserirNaHash() – insere associação pista/suspeito na tabela hash. */
HashTable *criarHash(size_t tamanho);
void inserirNaHash(HashTable *ht, const char *chave, const char *valor);
char *encontrarSuspeito(HashTable *ht, const char *chave);

/* encontrarSuspeito() – consulta o suspeito correspondente a uma pista. */
/* (usa encontrarSuspeito function acima) */

/* verificarSuspeitoFinal() – conduz à fase de julgamento final. */
void verificarSuspeitoFinal(PistaNode *pistasColetadas, HashTable *hash);

/* Funções utilitárias */
const char *getClueForRoom(const char *roomName);
void liberarSalas(Sala *raiz);
void liberarPistas(PistaNode *raiz);
void liberarHash(HashTable *ht);
char *strdup_safe(const char *s);
void limparBufferEntrada(void);
char readOption();

/* ---------------------------
   Implementações
   ---------------------------*/

/* criarSala() – cria dinamicamente um cômodo.
   Aloca memória para uma Sala, duplica o nome e inicializa ponteiros. */
Sala *criarSala(const char *nome) {
    Sala *s = (Sala *)malloc(sizeof(Sala));
    if (!s) {
        fprintf(stderr, "Erro de alocação criarSala\n");
        exit(EXIT_FAILURE);
    }
    s->nome = strdup_safe(nome);
    s->esq = s->dir = NULL;
    return s;
}

/* inserirPista() – insere a pista coletada na árvore BST de pistas (se ainda não existir).
   Retorna a nova raiz da BST. */
PistaNode *inserirPista(PistaNode *raiz, const char *pista) {
    if (!raiz) {
        PistaNode *n = (PistaNode *)malloc(sizeof(PistaNode));
        if (!n) { fprintf(stderr, "Erro alocar PistaNode\n"); exit(EXIT_FAILURE); }
        n->pista = strdup_safe(pista);
        n->esq = n->dir = NULL;
        return n;
    }
    int cmp = strcmp(pista, raiz->pista);
    if (cmp == 0) {
        // já existe, não inserir duplicata
        return raiz;
    } else if (cmp < 0) {
        raiz->esq = inserirPista(raiz->esq, pista);
    } else {
        raiz->dir = inserirPista(raiz->dir, pista);
    }
    return raiz;
}

/* existePista() – verifica se já temos a pista na BST */
int existePista(PistaNode *raiz, const char *pista) {
    if (!raiz) return 0;
    int cmp = strcmp(pista, raiz->pista);
    if (cmp == 0) return 1;
    if (cmp < 0) return existePista(raiz->esq, pista);
    return existePista(raiz->dir, pista);
}

/* listarPistasInorder() – lista as pistas em ordem alfabética */
void listarPistasInorder(PistaNode *raiz) {
    if (!raiz) return;
    listarPistasInorder(raiz->esq);
    printf(" - %s\n", raiz->pista);
    listarPistasInorder(raiz->dir);
}

/* criarHash() – inicializa a tabela hash com bucket count */
HashTable *criarHash(size_t tamanho) {
    HashTable *ht = (HashTable *)malloc(sizeof(HashTable));
    if (!ht) { fprintf(stderr, "Erro alocar HashTable\n"); exit(EXIT_FAILURE); }
    ht->tamanho = tamanho;
    ht->buckets = (HashEntry **)calloc(tamanho, sizeof(HashEntry *));
    if (!ht->buckets) { fprintf(stderr, "Erro alocar buckets\n"); exit(EXIT_FAILURE); }
    return ht;
}

/* Função hash simples (djbx33x-like) */
static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (unsigned char)c; /* hash * 33 + c */
    return hash;
}

/* inserirNaHash() – insere associação pista/suspeito na tabela hash.
   Se a chave já existir, substitui o valor. */
void inserirNaHash(HashTable *ht, const char *chave, const char *valor) {
    if (!ht || !chave) return;
    unsigned long h = hash_str(chave) % ht->tamanho;
    HashEntry *cur = ht->buckets[h];
    while (cur) {
        if (strcmp(cur->chave, chave) == 0) {
            // substitui valor existente
            free(cur->valor);
            cur->valor = strdup_safe(valor);
            return;
        }
        cur = cur->prox;
    }
    // inserir novo
    HashEntry *e = (HashEntry *)malloc(sizeof(HashEntry));
    if (!e) { fprintf(stderr, "Erro alocar HashEntry\n"); exit(EXIT_FAILURE); }
    e->chave = strdup_safe(chave);
    e->valor = strdup_safe(valor);
    e->prox = ht->buckets[h];
    ht->buckets[h] = e;
}

/* encontrarSuspeito() – consulta o suspeito correspondente a uma pista.
   Retorna ponteiro para string (não modificar). Retorna NULL se não achar. */
char *encontrarSuspeito(HashTable *ht, const char *chave) {
    if (!ht || !chave) return NULL;
    unsigned long h = hash_str(chave) % ht->tamanho;
    HashEntry *cur = ht->buckets[h];
    while (cur) {
        if (strcmp(cur->chave, chave) == 0) return cur->valor;
        cur = cur->prox;
    }
    return NULL;
}

/* verificarSuspeitoFinal() – conduz à fase de julgamento final.
   Lista as pistas coletadas, pede ao jogador o nome do suspeito e verifica
   se ao menos duas pistas apontam para esse suspeito.
*/
void verificarSuspeitoFinal(PistaNode *pistasColetadas, HashTable *hash) {
    printf("\n====== Fase de Acusação Final ======\n\n");
    if (!pistasColetadas) {
        printf("Você não coletou nenhuma pista. Não há como acusar ninguém.\n");
        return;
    }
    printf("Pistas coletadas:\n");
    listarPistasInorder(pistasColetadas);

    char acusado[128];
    printf("\nDigite o nome do suspeito que deseja acusar: ");
    if (!fgets(acusado, sizeof(acusado), stdin)) {
        printf("Erro leitura.\n");
        return;
    }
    // remover newline
    acusado[strcspn(acusado, "\r\n")] = '\0';
    // trim spaces (simples)
    while (strlen(acusado) && isspace((unsigned char)acusado[strlen(acusado)-1])) acusado[strlen(acusado)-1] = '\0';
    char *p = acusado;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') {
        printf("Nome inválido.\n");
        return;
    }

    // Contar quantas pistas apontam para o acusado:
    // para isso, percorremos a BST e para cada pista consultamos a hash
    int count = 0;

    // função interna recursiva
    void contar(PistaNode *n) {
        if (!n) return;
        contar(n->esq);
        char *sus = encontrarSuspeito(hash, n->pista);
        if (sus && strcasecmp(sus, p) == 0) count++;
        contar(n->dir);
    }
    contar(pistasColetadas);

    printf("\nVocê acusou: %s\n", p);
    printf("Número de pistas que apontam para %s: %d\n", p, count);
    if (count >= 2) {
        printf("\n>> Acusação aceita: Há evidências suficientes para incriminar %s.\n", p);
        printf("Parabéns, detetive! Você resolveu o caso.\n");
    } else {
        printf("\n>> Acusação rejeitada: Não há pistas suficientes contra %s.\n", p);
        printf("O verdadeiro culpado pode estar em outro lugar... continue investigando.\n");
    }
}

/* explorarSalas() – navega pela árvore e ativa o sistema de pistas.
   O jogador pode escolher: esquerda (e), direita (d) ou sair (s).
   Ao visitar uma sala, identifica-se uma pista (se houver) e a armazena
   na BST de pistas (se ainda não coletada). */
void explorarSalas(Sala *raiz, PistaNode **pistasColetadas, HashTable *hash) {
    if (!raiz) return;
    Sala *atual = raiz;
    printf("Você está na mansão. Comece a explorar!\n");
    printf("Comandos: (e) esquerda | (d) direita | (s) sair/terminar exploração\n\n");

    int visitas = 0;
    // Visitar sala inicial
    while (1) {
        printf("Sala atual: %s\n", atual->nome);
        const char *pista = getClueForRoom(atual->nome);
        if (pista) {
            printf("Há uma pista nesta sala: \"%s\"\n", pista);
            // inserir se ainda não coletada
            if (!existePista(*pistasColetadas, pista)) {
                *pistasColetadas = inserirPista(*pistasColetadas, pista);
                // (A tabela hash já deve conter a associação pista->suspeito
                //  criada no início do jogo; aqui só confirmamos que existe)
                char *sus = encontrarSuspeito(hash, pista);
                if (sus) {
                    printf("-> Esta pista aponta para: %s (associação registrada).\n", sus);
                } else {
                    printf("-> Esta pista não possui associação com suspeito definida.\n");
                }
            } else {
                printf("Você já coletou essa pista anteriormente — não será duplicada.\n");
            }
        } else {
            printf("Não há pistas visíveis aqui.\n");
        }

        // pegar comando do jogador
        printf("\nEscolha (e/d/s): ");
        char op = readOption();
        if (op == 's') {
            printf("Você decidiu encerrar a exploração.\n");
            break;
        } else if (op == 'e') {
            if (atual->esq) {
                atual = atual->esq;
            } else {
                printf("Não há sala à esquerda. Escolha outra ação.\n");
            }
        } else if (op == 'd') {
            if (atual->dir) {
                atual = atual->dir;
            } else {
                printf("Não há sala à direita. Escolha outra ação.\n");
            }
        }
        visitas++;
        printf("\n-----------------------------\n");
    }

    printf("\nExploração finalizada. Você coletou as pistas disponíveis durante a visita.\n");
}

/* getClueForRoom() – regras lógicas que mapeiam nome da sala -> pista.
   Retorna ponteiro constante para string literal (não liberar). */
const char *getClueForRoom(const char *roomName) {
    if (!roomName) return NULL;
    if (strcasecmp(roomName, "Hall") == 0) {
        return "Pegada encharcada";
    } else if (strcasecmp(roomName, "Biblioteca") == 0) {
        return "Livro com anotações sobre dívidas";
    } else if (strcasecmp(roomName, "Escritorio") == 0) {
        return "Caneta quebrada com tinta azul";
    } else if (strcasecmp(roomName, "Cozinha") == 0) {
        return "Fio de cabelo ruivo no avental";
    } else if (strcasecmp(roomName, "Despensa") == 0) {
        return "Frasco de veneno vazio";
    } else if (strcasecmp(roomName, "Jardim") == 0) {
        return "Pegada de luva de jardinagem";
    } else if (strcasecmp(roomName, "Conservatorio") == 0) {
        return "Folha de partitura com manchas";
    } else if (strcasecmp(roomName, "Sala de Estar") == 0) {
        return "Copo com resquício de uísque";
    } else if (strcasecmp(roomName, "Quarto") == 0) {
        return "Brinco dourado quebrado";
    }
    return NULL;
}

/* Funções de liberação de memória */
void liberarSalas(Sala *raiz) {
    if (!raiz) return;
    liberarSalas(raiz->esq);
    liberarSalas(raiz->dir);
    free(raiz->nome);
    free(raiz);
}
void liberarPistas(PistaNode *raiz) {
    if (!raiz) return;
    liberarPistas(raiz->esq);
    liberarPistas(raiz->dir);
    free(raiz->pista);
    free(raiz);
}
void liberarHash(HashTable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->tamanho; ++i) {
        HashEntry *e = ht->buckets[i];
        while (e) {
            HashEntry *next = e->prox;
            free(e->chave);
            free(e->valor);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

/* strdup wrapper segura */
char *strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (!r) { fprintf(stderr, "Erro strdup_safe\n"); exit(EXIT_FAILURE); }
    memcpy(r, s, n);
    return r;
}

/* Limpa qualquer resto de entrada no stdin */
void limparBufferEntrada(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* Lê uma opção do usuário (primeiro caractere não-espaço)
   retorna em minúscula. */
char readOption() {
    int c;
    // ler até encontrar um caractere visível
    do {
        c = getchar();
        if (c == EOF) return 's';
    } while (isspace(c));
    // consumir resto da linha
    limparBufferEntrada();
    char ch = (char)tolower(c);
    if (ch != 'e' && ch != 'd' && ch != 's') {
        return 'x'; // inválido
    }
    return ch;
}

/* ---------------------------
   Main: montagem do mapa e loop principal
   ---------------------------*/
int main(void) {
    printf("====== Detective Quest (Enigma Studios) ======\n\n");

    /* Montar mapa da mansão (árvore binária fixa) */
    /* Estrutura de exemplo:
                Hall
               /    \
         Biblioteca  Cozinha
          /    \       /   \
      Escritorio Sala  Despensa Jardim
                 Estar
    */
    Sala *hall = criarSala("Hall");
    Sala *biblioteca = criarSala("Biblioteca");
    Sala *cozinha = criarSala("Cozinha");
    Sala *escritorio = criarSala("Escritorio");
    Sala *salaEstar = criarSala("Sala de Estar");
    Sala *despensa = criarSala("Despensa");
    Sala *jardim = criarSala("Jardim");
    Sala *quarto = criarSala("Quarto");
    Sala *conservatorio = criarSala("Conservatorio");

    hall->esq = biblioteca;
    hall->dir = cozinha;

    biblioteca->esq = escritorio;
    biblioteca->dir = salaEstar;

    cozinha->esq = despensa;
    cozinha->dir = jardim;

    salaEstar->esq = quarto;
    salaEstar->dir = conservatorio;

    /* Criar tabela hash e inserir mapeamentos pista -> suspeito. */
    HashTable *hash = criarHash(101); // 101 buckets (primo simples)
    // Associações definidas pelo design do jogo:
    inserirNaHash(hash, "Pegada encharcada", "Jardineiro");
    inserirNaHash(hash, "Livro com anotações sobre dívidas", "Governanta");
    inserirNaHash(hash, "Caneta quebrada com tinta azul", "Advogado");
    inserirNaHash(hash, "Fio de cabelo ruivo no avental", "Cozinheira");
    inserirNaHash(hash, "Frasco de veneno vazio", "Quimico");
    inserirNaHash(hash, "Pegada de luva de jardinagem", "Jardineiro");
    inserirNaHash(hash, "Folha de partitura com manchas", "Pianista");
    inserirNaHash(hash, "Copo com resquício de uísque", "Motorista");
    inserirNaHash(hash, "Brinco dourado quebrado", "Herdeira");

    /* Observação: algumas pistas apontam para o mesmo suspeito (ex.: jardineiro),
       o que permite que duas ou mais pistas sustentem a acusação. */

    /* BST vazia para armazenar pistas coletadas */
    PistaNode *pistasColetadas = NULL;

    /* Instruções iniciais */
    printf("Bem-vindo(a) detetive! Explore a mansão e colete pistas.\n");
    printf("Ao visitar uma sala, se houver uma pista, ela será automaticamente coletada (sem duplicatas).\n\n");

    /* Iniciar exploração interativa */
    explorarSalas(hall, &pistasColetadas, hash);

    /* Fase final: julgamento */
    verificarSuspeitoFinal(pistasColetadas, hash);

    /* Limpeza de memória */
    liberarPistas(pistasColetadas);
    liberarHash(hash);
    liberarSalas(hall);

    printf("\nObrigado por jogar Detective Quest!\n");
    return 0;
}
