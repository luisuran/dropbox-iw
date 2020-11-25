#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFERLEN 1024
#define h_addr h_addr_list[0]

void send_file(char *filename, int sockfd);
void receive_file(char *filename, int sockfd);

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("%s <host> <puerto>\n", argv[0]);
        return 1;
    }

    int sockfd, port;
    struct sockaddr_in server_addr;
    struct hostent *server;

    port = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM es tcp
    if (sockfd < 0)
    {
        perror("Error abriendo el socket");
        return 1;
    }

    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        perror("Host inválido");
        return 1;
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length); // bcopy copia n bytes de server(hostent) a serv_addr(sockaddr)
    server_addr.sin_port = htons(port);

    //Establecemos la conexión
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Fallo en la conexión");
        return 1;
    }

    //////////////////////////////////////////// LOGIN ////////////////////////////////////////////
    puts("Bienvenido a dropbox-iw");

    while (1)
    {
        char *name = (char *)malloc(BUFFERLEN);
        char *password = (char *)malloc(BUFFERLEN);

        printf("\nIngrese nombre de usuario: ");
        fgets(name, BUFFERLEN, stdin);
        name[strlen(name) - 1] = '\0';

        printf("Ingrese contraseña: ");
        fgets(password, BUFFERLEN, stdin);
        password[strlen(password) - 1] = '\0';

        send(sockfd, name, BUFFERLEN, 0);
        send(sockfd, password, BUFFERLEN, 0);

        char code[BUFFERLEN];
        if (recv(sockfd, &code, BUFFERLEN, 0) == -1)
        {
            perror("Error recibiendo código de login");
            close(sockfd);
            return 1;
        }

        if (strcmp(code, "NO") == 0)
        {
            printf("\nContraseña incorrecta. Vuelva a intentar.\n");
            continue;
        }
        break;
    }

    //////////////////////////////////////////// MENU ////////////////////////////////////////////
    printf("\nElija una opción:\n");
    puts("LISTAR - Muestra archivos guardados");
    puts("CARGAR <archivo> - Sube un archivo al servidor");
    puts("DESCARGAR <archivo> - Descarga un archivo del servidor");
    puts("ELIMINAR <archivo> - Elimina un archivo del servidor");
    printf("SALIR - Terminar la sesión\n\n");

    char *texto = (char *)malloc(BUFFERLEN);

    while (1)
    {
        fgets(texto, BUFFERLEN, stdin);
        texto[strlen(texto) - 1] = '\0';

        send(sockfd, texto, BUFFERLEN, 0);

        char *command;
        char *filename;

        command = strtok(texto, " ");

        // Cierro la sesión
        if (strcmp(command, "SALIR") == 0)
            break;
        // Muestro la lista de archivos
        else if (strcmp(command, "LISTAR") == 0)
        {
            char file_list[BUFFERLEN];
            if (recv(sockfd, &file_list, BUFFERLEN, 0) == -1)
            {
                perror("Error recibiendo lista de archivos");
                close(sockfd);
                return 1;
            }
            puts(file_list);
        }
        // Cargo un archivo en el servidor
        else if (strcmp(command, "CARGAR") == 0) // Chequeo si el comando es CARGAR
        {
            filename = strtok(NULL, " "); // Obtengo el nombre del archivo a enviar

            send_file(filename, sockfd);

            printf("Archivo enviado exitosamente\n\n");
        }
        // Descargo un archivo del servidor
        else if (strcmp(command, "DESCARGAR") == 0)
        {
            filename = strtok(NULL, " ");

            send(sockfd, filename, BUFFERLEN, 0); // Envío el nombre del archivo a descargar

            receive_file(filename, sockfd); // Recibo el archivo desde el servidor

            printf("Archivo descargado exitosamente\n\n");
        }
        // Elimino un archivo del servidor
        else if (strcmp(command, "ELIMINAR") == 0)
        {
            filename = strtok(NULL, " ");

            send(sockfd, filename, BUFFERLEN, 0); // Envío el nombre del archivo a eliminar

            printf("Archivo eliminado exitosamente\n\n");
        }
        else
            printf("Comando incorrecto\n\n");
    }

    close(sockfd);

    return 0;
}

void send_file(char *filename, int sockfd)
{
    FILE *fp;
    char data[BUFFERLEN] = {0};

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("Error abriendo el archivo.");
        exit(1);
    }

    fread(&data, BUFFERLEN, 1, fp);

    if (send(sockfd, data, sizeof(data), 0) == -1)
    {
        perror("Error enviando el archivo.");
        exit(1);
    }

    fclose(fp);
}

void receive_file(char *filename, int sockfd)
{
    FILE *fp;
    char buffer[BUFFERLEN];

    fp = fopen(filename, "w");

    if (recv(sockfd, buffer, BUFFERLEN, 0) == -1)
    {
        perror("Error recibiendo el archivo.");
        exit(1);
    }

    fwrite(buffer, 1, strlen(buffer), fp);

    fclose(fp);
}
