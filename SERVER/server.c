#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFFERLEN 1024

char *get_filename_ext(char *filename);
char *get_txt_files(char *uri);
void receive_file(char *filename, int sockfd, char *username);
void send_file(char *filename, int sockfd, char *username);
char *login(char *name, char *password);

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("%s <puerto>\n", argv[0]);
        return 1;
    }

    pid_t childpid;

    int sockfd_serv, sockfd_cli, port;
    struct sockaddr_in server_addr, client_addr;

    sockfd_serv = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_serv == -1)
    {
        perror("Error abriendo el socket");
        exit(1);
    }

    bzero((char *)&server_addr, sizeof(server_addr));

    port = atoi(argv[1]);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //Asignamos un puerto al socket
    if (bind(sockfd_serv, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Error al asociar el puerto a la conexión");
        close(sockfd_serv);
        return 1;
    }

    //Ponemos el servidor a escuchar
    listen(sockfd_serv, 5);
    printf("Escuchando en el puerto %d\n\n", ntohs(server_addr.sin_port));

    while (1)
    {
        socklen_t long_cli = sizeof(client_addr);
        //Aceptamos la conexión de un cliente
        sockfd_cli = accept(sockfd_serv, (struct sockaddr *)&client_addr, &long_cli);
        if (sockfd_cli == -1)
        {
            perror("Error al aceptar la conexión");
            close(sockfd_serv);
            return 1;
        }

        printf("Conectado con %s:%d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));

        childpid = fork();
        if (childpid == 0)
        {
            int pid = getpid(); //Guardo el id del proceso hijo

            //////////////////////////////////////////// LOGIN ////////////////////////////////////////////
            char name[BUFFERLEN];
            char password[BUFFERLEN];
            while (1)
            {

                if (recv(sockfd_cli, &name, BUFFERLEN, 0) == -1)
                {
                    perror("Name error");
                    close(sockfd_serv);
                    return 1;
                }

                if (recv(sockfd_cli, &password, BUFFERLEN, 0) == -1)
                {
                    perror("Name error");
                    close(sockfd_serv);
                    return 1;
                }

                char code[BUFFERLEN];
                strcpy(code, login(name, password)); // Obtengo el código de login (OK / NO)

                send(sockfd_cli, &code, BUFFERLEN, 0);

                if (strcmp(code, "NO") == 0) //Contraseña incorrecta
                    continue;
                break;
            }

            //////////////////////////////////////////// MENU ////////////////////////////////////////////
            while (1)
            {
                // close(sockfd_serv);

                char texto[BUFFERLEN];
                char filename[BUFFERLEN];

                if (recv(sockfd_cli, &texto, BUFFERLEN, 0) == -1)
                {
                    perror("Error recibiendo");
                    close(sockfd_serv);
                    return 1;
                }

                // Cierro la sesión
                if (strcmp(texto, "SALIR") == 0)
                    break;
                // Muestro la lista de archivos
                else if (strcmp(texto, "LISTAR") == 0)
                {
                    char file_list[1024];

                    get_txt_files(name); //Ésta línea arregla el problema de que añadía texto a la lista de archivos
                    strcpy(file_list, get_txt_files(name));

                    send(sockfd_cli, &file_list, BUFFERLEN, 0);
                }
                // Cargo un archivo en el servidor
                else if (strcmp(strtok(texto, " "), "CARGAR") == 0) // Chequeo si la primera palabra del comando es CARGAR
                {
                    strcpy(filename, strtok(NULL, " ")); // Obtengo el nombre del archivo a enviar

                    receive_file(filename, sockfd_cli, name);
                }
                // Descargo un archivo del servidor
                else if (strcmp(strtok(texto, " "), "DESCARGAR") == 0) // Chequeo si la primera palabra del comando es DESCARGAR
                {
                    if (recv(sockfd_cli, &filename, BUFFERLEN, 0) == -1)
                    {
                        perror("Error recibiendo nombre del archivo a descargar");
                        close(sockfd_serv);
                        return 1;
                    }

                    send_file(filename, sockfd_cli, name);
                }
                // Elimino un archivo del servidor
                else if (strcmp(strtok(texto, " "), "ELIMINAR") == 0) // Chequeo si la primera palabra del comando es DESCARGAR
                {
                    if (recv(sockfd_cli, &filename, BUFFERLEN, 0) == -1)
                    {
                        perror("Error recibiendo nombre del archivo a eliminar");
                        close(sockfd_serv);
                        return 1;
                    }

                    char dir_filename[BUFFERLEN];

                    strcpy(dir_filename, name);
                    strcat(dir_filename, "/");
                    strcat(dir_filename, filename);

                    remove(dir_filename); // Elimino el archivo
                }
            }

            kill(pid, SIGKILL);
        }
    }
    return 0;
}

char *get_filename_ext(char *filename)
{
    char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

char *get_txt_files(char *uri)
{
    char *files = malloc(BUFFERLEN); // Lista de ficheros a retornar

    DIR *d;
    struct dirent *dir;
    d = opendir(uri);

    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            char *extension = get_filename_ext(dir->d_name); // Obtengo la extensión del archivo

            if (strcmp(extension, "txt") == 0 && strcmp(dir->d_name, "data.txt")) // Obtengo sólo los archivos con extensión txt
                                                                                  // Excepto el archivo data.txt
            {
                dir->d_name[strlen(dir->d_name)] = '\n'; // Añado separación a los nombres de los archivos
                strcat(files, dir->d_name);
            }
        }
        closedir(d);
    }

    return files;
}

void receive_file(char *filename, int sockfd, char *username)
// Toma como parámetros el nombre del archivo, el socket y el nombre de usuario
{
    char dir_filename[BUFFERLEN];

    strcpy(dir_filename, username);
    strcat(dir_filename, "/");
    strcat(dir_filename, filename);

    FILE *fp;
    char buffer[BUFFERLEN];

    fp = fopen(dir_filename, "w");

    if (recv(sockfd, buffer, BUFFERLEN, 0) == -1)
    {
        perror("Error recibiendo el archivo.");
        exit(1);
    }

    fwrite(buffer, 1, strlen(buffer), fp);

    fclose(fp);
}

void send_file(char *filename, int sockfd, char *username)
{
    char dir_filename[BUFFERLEN];

    strcpy(dir_filename, username);
    strcat(dir_filename, "/");
    strcat(dir_filename, filename);

    FILE *fp;
    char data[BUFFERLEN] = {0};

    fp = fopen(dir_filename, "r");
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

char *login(char *name, char *password)
{
    char base_dir[BUFFERLEN];

    strcpy(base_dir, name); //Guardo una copia del nombre para utilizarlo como directorio base

    // Creo una carpeta con el nombre del usuario (si es que no existe ya)
    if (mkdir(base_dir, 0777) == 0)
    {
        // Creo el archivo data.txt que contiene la contraseña del usuario
        FILE *fp;
        strcat(base_dir, "/data.txt");
        fp = fopen(base_dir, "w");
        fputs(password, fp);
        fclose(fp);
    }
    else
    {
        char buffer[BUFFERLEN];

        FILE *fp;
        strcat(base_dir, "/data.txt");
        fp = fopen(base_dir, "r");
        fgets(buffer, 50, fp);
        fclose(fp);

        if (strcmp(buffer, password) != 0) // Comparo la contraseña ingresada con la guardada

            return "NO";
    }
    return "OK";
}