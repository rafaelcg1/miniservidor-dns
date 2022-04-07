#include "common.h" //biblioteca de funcoes comuns

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>

#define BUFSZ 1024
#define version "v4" //versao optada a ser utilizada, também cabe o uso da IPV6, basta alterar para "v6"

int size_dict;
int count_connections;
lista dict[BUFSZ];


void usage(int argc, char **argv) {
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

// Compara se dois hostnames sao iguais
bool compareHost (const char host1[], const char host2[]) {      
    int comp = 0;      
    while (host1[comp] == host2[comp] && host1[comp] != '\0' && host2[comp] !='\0') {
        ++comp;                
    }
    return host1[comp] == '\0' && host2[comp] == '\0';
}

// Busca um hostname na lista e retorna sua posicao
int searchHost (lista dict[], const char hostname[], int size_dict) {      

    bool compareHost (const char host1[], const char host2[]);       
    int comp = 0;      
    while (comp < size_dict) {         
        if (compareHost(hostname, dict[comp].hostname)) {                
            return comp;               
        } else {                
             ++comp;               
        }       
    }
    return -1;  
}


// Divide a string lida na linha de comando por meio de um delimitador
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_delim = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

   // Conta quantos elementos serao extraidos da string
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_delim = tmp;
        }
        tmp++;
    }

    // Adiciona um espaco para o ultimo delimitador
    count += last_delim < (a_str + strlen(a_str) - 1);

    // Adiciona um espaco para o ultimo caracter
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int main(int argc, char *argv[]) {
    
    int i;
    servidor serv[BUFSZ];
    pthread_t thread_id;
    char buffer[BUFSZ];
    char hostname[BUFSZ];
    char answer[BUFSZ];
    char** comandos;

    if (argc < 2) {
        usage(argc, argv);
    } 
    
    // Cria a thread que lida com as conexoes feitas 
    if (pthread_create(&thread_id, NULL, startConnectionHandler, (void*) argv[1]) < 0) {
        logexit("Erro ao criar a thread");
        return 1;
    }

    size_dict = 0;
    count_connections = 0;

    while (1) {
        printf ("Entre com um comando:\n");
        // Lê linha de comando ate uma quebra de linha
        scanf("%[^\n]s",&buffer);
        setbuf(stdin, NULL);

        // Separa comandos inseridos na linha de comando por espaco
        comandos = str_split(buffer, ' ');
        if (!strcmp(*(comandos), "add")){   // Adiciona hostname a lista local
            
            memset(hostname, 0, BUFSZ);
            sprintf(hostname, "%s", *(comandos + 1));
            
            // Verifica se o hostname ja esta na lista local
            int search = searchHost(dict, hostname, size_dict);
            
            // Caso sim, o IP é atualizado
            if (search != -1) {
                sprintf(dict[search].ip, "%s", *(comandos + 2));
                printf("Host %s atualizado com o IP: %s\n", dict[search].hostname, dict[search].ip);

            } else {    // Caso nao, adiciona o ip e o hostname a lista local
                sprintf(dict[size_dict].hostname, "%s", hostname);
                sprintf(dict[size_dict].ip, "%s", *(comandos + 2));
                printf("Host %s adicionado com o IP: %s\n", dict[size_dict].hostname, dict[size_dict].ip);

            }
            
            // aumenta o tamanho da lista
            size_dict++;
            
        } else if (!strcmp(*(comandos), "search")){

            memset(hostname, 0, BUFSZ);
            sprintf(hostname, "%s", *(comandos + 1));

            // verifica se o hostname ja esta na lista local
            int search = searchHost(dict, hostname, size_dict);
            
            memset(answer, 0, BUFSZ);
            if (search != -1) {   // se sim, ja o imprime na tela
                sprintf(answer, dict[search].ip);
                printf("IP encontrado: %s\n", answer);

            } else {    // se nao, envia requisicoes pelo hostname para os servidores conectados
                
                // realiza o parse do socket
                struct sockaddr_storage storage;
                if (0 != server_sockaddr_init(version, argv[1], &storage)) {
                    logexit("sockaddr_init");
                }

                // cria socket
                int s;
                s = socket(storage.ss_family, SOCK_DGRAM, 0);
                if (s == -1) {
                    logexit("Erro ao criar o socket");
                }

                // habilita o uso da mesma porta sucessivamente pelo servidor
                int enable = 1;
                if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
                    logexit("Erro ao configurar socket");
                }

                // bind
                struct sockaddr *addr = (struct sockaddr *)(&storage);
                if (0 != bind(s, addr, sizeof(storage))) {
                    logexit("Erro ao realizar bind do servidor");
                }

                // configura destino
                struct sockaddr_in to;
                
                if(count_connections == 0){  // nenhum servidor conectado na rede, ip nao encontrado
                    printf("IP não encontrado na rede.\n");
                }

                for (i = 0; i < count_connections; i++){
                    
                    // Cria mensagem de requisicao
                    sprintf(answer, "1");
                    strcat(answer, hostname);

                    uint16_t port = (uint16_t)atoi(serv[i].port); // unsigned short

                    // Direciona a mensagem para cada servidor conectado
                    memset(&to, 0, sizeof(to));
                    to.sin_family = AF_INET;
                    to.sin_addr.s_addr = inet_addr(serv[i].ip);
                    to.sin_port = htons(port);
                    
                    // manda requisicao para cada um dos servidores conectados
                    // thread servidor vai lidar com as respostas
                    size_t count = sendto(s, answer, strlen(answer) + 1, 0, (struct sockaddr*)&to, sizeof(to));
                    if (count != strlen(answer) + 1) {
                        logexit("Timeout: Enviando requisicao");
                    }

                }

            }

        } else if (!strcmp(*(comandos), "link")){   // Realiza o link entre outros servidores e o servidor que recebeu o comando

            // Endereco de cada servidor e armazenado em uma lista de servidores
            strcpy(serv[count_connections].ip, *(comandos + 1));
            strcpy(serv[count_connections].port, *(comandos + 2));
            count_connections++;

        }

        // Desaloca comandos
        if (comandos){
            for (i = 0; *(comandos + i); i++){
                free(*(comandos + i));
            }
            free(comandos);
        }

    }

    return 0;
}

// Thread que lida com as conexoes para o servidor, respondendo requisicoes e analisando respostas
void *startConnectionHandler(void *port) {

    int i;
    int tipo;
    
	//função que realiza o parse do socket
    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(version, port, &storage)) {
        logexit("sockaddr_init");
    }

    //cria um novo socket
    int s;
    s = socket(storage.ss_family, SOCK_DGRAM, 0);
    if (s == -1) {
        logexit("Erro ao criar o socket");
    }

    // habilita o uso da mesma porta sucessivamente pelo servidor
    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("Erro ao configurar socket");
    }

	//bind
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("Erro ao realizar bind do servidor");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("Iniciado em %s, aguardando comandos\n", addrstr);

    // Variavel que guarda a quantidade de respostas do servidor para finalizar a busca
    int count_answers = 0;

    while(1){

        char buf[BUFSZ];
        char hostname[BUFSZ];
        char letra[BUFSZ];
        char answer[BUFSZ];
        memset(buf, 0, BUFSZ);
        memset(hostname, 0, BUFSZ);
        memset(letra, 0, BUFSZ);
        memset(answer, 0, BUFSZ);

        // recebe mensagem
        struct sockaddr_in clientAddr;
        socklen_t addrlen = sizeof(clientAddr);
        size_t count = recvfrom(s, buf, BUFSZ - 1, 0, (struct sockaddr*)&clientAddr, &addrlen);
        if (count == -1){
            logexit("Timeout: Aguardando a requisicao");
        }

        // cast do tipo da mensagem recebida para int
        tipo = buf[0] - '0';
        if(tipo == 1){  // responde requisicao

            memset(hostname, 0, BUFSZ);
            for(i = 1; i < count - 1; i++){	// excluindo o tipo da mensagem para acessar o hostname
                memset(letra, 0, BUFSZ);
                sprintf(letra, "%c", buf[i]);
                strcat(hostname, letra);
            }

            // Verifica se o hostname já existe na lista local
            int search = searchHost(dict, hostname, size_dict);
            
            // Prepara a mensagem de resposta
            memset(answer, 0, BUFSZ);
            sprintf(answer, "2");
            
            // Caso sim, a resposta será o ip do hostname buscado
            if (search != -1) {
                strcat(answer, dict[search].ip);

            } else {    // Caso nao, a resposta será -1
                memset(letra, 0, BUFSZ);
                sprintf(letra, "%d", search);
                strcat(answer, letra);
            }

            // Envia mensagem de resposta
            count = sendto(s, answer, strlen(answer) + 1, 0, (struct sockaddr*)&clientAddr, addrlen);
            if (count != strlen(answer) + 1) {
                logexit("Timeout: Enviando resposta");
            }

        } else if(tipo == 2){   // Analisa resposta

            memset(answer, 0, BUFSZ);
            for(i = 1; i < count - 1; i++){	// Exclui o tipo da mensagem para ler a resposta
                memset(letra, 0, BUFSZ);
                sprintf(letra, "%c", buf[i]);
                strcat(answer, letra);
            }

            // Caso a resposta seja -1, o ip nao foi encontrado, adiciona um no contador de mensagens recebidas
            if (!strcmp(answer, "-1")){
                count_answers++;
            } else {    // Caso nao, apenas imprime o ip encontrado do hostname solicitado
                count_answers = 0;  // Zera respostas recebidas para proximo fazer um proximo search
                printf("O IP desse host foi encontrado: %s\n", answer);
            }
            if(count_answers == count_connections){    // Se o if for true, nenhum dos servidores possui o ip requisitado
                count_answers = 0;
                printf("Nao foi possivel encontrar o IP do host buscado\n");
            }

        }

    }
    exit(EXIT_SUCCESS);
    
}