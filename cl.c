//build: 1) gcc -S client.c     2)gcc -o client client.s arm_socket.c 

/*
//TODO: проверять результат strdup на NULL

//TODO: ошибка если слишком быстро набираем сообщения - программа зависает
//TODO: посимвольное стирание
//TODO: попробовать убрать echo в kbhit с помощью своего getch
//  пока что костыль: из kbhit убрать вывод символов посредством полного стирания экрана(как при выходе) на каждом такте цикла

//TODO: курсор почему-то не убирается(из-за ssh?)
//TODO: убрать test_login и тестовые выводы
//TODO: смайлики из DOS несколько байт, из-за этого проблемки



//TODO: исправить следующие ошибки тем, чтобы запретить отправку самому себе
/*TODO: ошибки снизу происходят только когда в сообщении сначала цифры или символов до цифр меньше 5 штук: abcde123- сломается, abcdef123 - норм работает
//При отпраке сообщени  длиной 6 просто зависает и завершает работу потом
//При отпраке сообщени  длиной 7 ошибка: stack smashing detected *** unknown terminated aborted (core dumped)
//При отправке себе сообщения длиной 8 и более, а затем при получении sigfault
//а при отправке длинного сообщения оно начинает перезатирать имя получателя




////======исправлено?


//(вроде исправлено)
//TODO: при получении сообщения происходит что-то странное(в том числе и пустого сообщения, то есть если нет сообщений)
// либо программа как-то странно перестает отвечать, либо одно сообщения два раза выводится и при прокрутке первое исчезает
// возможно это связано с получением сообщений от русского имени(вроде как нет) -или с тем, что в receive я лишний раз делаю malloc



//TODO: когда отправляем русскую букву, первый символ равен вопросу и после отправки программа завершается. Но это если русский символ с начала
//TODO: когда вводим русское сообщение, то граница съезжает, а также максимальная длина уменьшается
//Ошибки выше исправляются исправлением функции kbhit - если русскйи символ, то делаем getchar 2-ой раз, и оба символа запихиваем

*/




#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "arm_socket.h"

#include <termios.h>
#include <fcntl.h>

#include <unistd.h>
//^for own getch
/*
**В общении с сервером перед каждым arm_recv нужно занулять буфер, чтобы гарантировано обнулить прошлую информацию
** Длина окна = 80
** Высота окна = 24 символа
**Справка: вверх-вниз = 1 и 2; пробел вместо Enter; ESC - выход; TAB - переключить окна; "/" - отправить всем
**
**
**
*/


#define TOTAL_LENGTH 82
#define TOTAL_HEIGHT 24
#define MSG_WIN_LEN 70
#define MSG_WIN_HEIGHT 14

#define KEY_UP 4 //это флаги, а не коды кнопок
#define KEY_DOWN 5

//#define ANSI_COLOR_RED        "\x1b[31m"
//#define ANSI_COLOR_GREEN  "\x31b[32m"
//#define ANSI_COLOR_RESET  "\x1b[0m"

//#define HIDE_CURSOR   "\e[?2c"
//#define SHOW_CURSOR "\e[?1c"

//#define HIDE_CURSOR "\x1b[?251"
//#define SHOW_CURSOR "\x1b[?25h"

#define CLEAR_SCREEN "\e[1;1H\e[2J"

#define ANSI_COLOR_RED      "\33[0;31m"
#define ANSI_COLOR_GREEN    "\33[0;32m"
#define ANSI_COLOR_YELLOW   "\33[0;33m"
#define ANSI_COLOR_BLUE     "\33[0;34m"
#define ANSI_COLOR_MAGENTA  "\33[0;35m"
#define ANSI_COLOR_KCYN     "\33[0;36m"
#define ANSI_COLOR_RESET    "\33[0m"

#define KEY_UP_CODE 49 //1
#define KEY_DOWN_CODE 50 //2
#define ENTER_CODE 10
//#define ENTER_CODE 32 //32 == ПРОБЕЛ
#define TAB_CODE 9
#define ESC_CODE 27

//627 либо 167 - это значок тильды на маке, бекспейс не работает почему-то
#define BACKSPACE_CODE 627 //вместо бекспейса пока использую плюсик(61). Бекспейс = 127. Но бекйспейс не работает
#define SECOND_BACKSPACE 167

#define ESC_CODE 27
#define SEND_ALL_CODE 47    //  символ / == 47
 
#define INPUT_WINDOW_NUM 0
#define PEOPLE_WINDOW_NUM 1
#define MESSAGES_WINDOW_NUM 2

#define MAX_USERS_LENGTH 50
#define MAX_MESSAGES_LENGTH 50

const int WINDOWS_TOTAL = 3;
const int MAX_MESSAGE_LEN = MSG_WIN_LEN;
const int MAX_PEOPLE_LEN = TOTAL_LENGTH - MSG_WIN_LEN;
const int MAX_PEOPLE_HEIGHT = TOTAL_HEIGHT - 2;
const int MAX_MESSAGE_HEIGHT = MSG_WIN_HEIGHT - 2;

const int MESSAGE_WINDOW_LENGTH = MSG_WIN_LEN;
const int MESSAGE_WINDOW_HEIGHT = MSG_WIN_HEIGHT;
const int PEOPLE_WINDOW_LENGTH = TOTAL_LENGTH - MSG_WIN_LEN;
const int PEOPLE_WINDOW_HEIGHT = TOTAL_HEIGHT;
const int INPUT_WINDOW_LENGTH = MSG_WIN_LEN;
const int INPUT_WINDOW_HEIGHT = TOTAL_HEIGHT - MSG_WIN_HEIGHT;

//static const int MAX_USERS_LENGTH = 50;
//static const int MAX_MESSAGES_LENGTH = 50;
const int MSG_MAX_LENGTH = 60;

int redrawScreen = 1;
int USERS_TOTAL = 0;
int MESSAGESS_TOTAL = 0;

int currentWindow = PEOPLE_WINDOW_NUM;
int pointerToUser = 0;
int pointerToMessage = 0;
int chosenUsersCount = 0;
int keyCode = 0;
int secondKeyCode = 0;
char charPressed = '\0';

char* additionalInfo = "someInfoHere";          //TODO fill
char* messageToSend = "";   //TODO fill
char* bound = "+";
char* delimeter = " ";



//Размер будет равен MSG_WIN_HEIGHT * 8, так как каждый элемент - это указатель длиной 1 байт. Например если размер указать 3, то sizeof == 24
   //Все элементы сами занулятся
char* chosenUsers[MAX_USERS_LENGTH] = {0};

char* users[MAX_USERS_LENGTH] = {0};

char* messages[MAX_MESSAGES_LENGTH] = {0};

char* authors[MAX_MESSAGES_LENGTH] = {0};

int PORT = 31337;
char IP[] = "192.168.10.201"; //local: 10.201
char buffer[1000];   //TODO: make buffer = 60 or check length before receiving
char test_buffer[1000];
char* login = "aahatov";
char* test_login = "test_login";
char* spravka = "Keys: 1;2;TAB;ESC;ENTER;/ ->(sendall);~ ->(delete msg)";
int sock;





///////=========================================server util methods===============================================//////////////
int actual_strlen(char* str) {
    int rus_1 = 0xd0;
    int rus_2 = 0xd1;

    int byteLength = strlen(str);
    for(int i = 0; i < strlen(str); i++) {
            //потенциально первая часть русского символа
            if(((int)str[i] & 0xff) == rus_1 || ((int)str[i] & 0xff)== rus_2) {
                byteLength -= 1;
                i += 1;
        }

    }

    return byteLength;
}

int isUserInList(char* user, char** array, int arr_len) {
    for(int i = 0; i < arr_len; i++) {
        if(array[i] == NULL) {
            break;
        }
        if(strcmp(user, array[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void deleteUsers(char** new_users, int new_count) {
    //Сначала удаляем из основных, затем из выбранных

    for(int i = 0; i < USERS_TOTAL; i++) {
        if(users[i] == NULL) {
            break;
        }
        if(!isUserInList(users[i], new_users, new_count)) {
            if(i == pointerToUser) {
                pointerToUser -= 1;
                if(pointerToUser < 0) {
                    pointerToUser = 0;
                }
            }
            for(int k = i; k < USERS_TOTAL; k++) {
                if(users[k] != NULL) {
                    users[k] = users[k+1];
                } else {
                    users[k-1] = NULL;
                    break;
                }
            }
            USERS_TOTAL -= 1;
            redrawScreen = 1;
        }
    }

    for(int i = 0; i < chosenUsersCount; i++) {
        if(chosenUsers[i] == NULL) {
            break;
        }
        if(!isUserInList(chosenUsers[i], new_users, new_count)) {
            for(int k = i; k < chosenUsersCount; k++) {
                if(chosenUsers[k] != NULL) {
                    chosenUsers[k] = chosenUsers[k+1];
                } else {
                    users[k-1] = NULL;
                    break;
                }
            }
            chosenUsersCount -= 1;
            redrawScreen = 1;
        }
    }

  /*  printf("After deleting USERS_TOTAL NOW: %d ", USERS_TOTAL);
    for(int i = 0; i < USERS_TOTAL; i++) {
        printf(" user: %s ;;", users[i]);
    }
    printf("\n"); */
}

void updateUsers(char** new_users, int new_count) {
    //TODO:
    //1)Удалить пользователей из общего числа и из выбранных, если они в данный момент отсутствуют в чате
    //2)С помощью ifUserInList получить уникальных пользователей и добавить их в массив

    //1)
    deleteUsers(new_users, new_count);

    //2)
    
    char* unical_users[MAX_USERS_LENGTH];
    for(int i = 0; i < new_count; i++) {
        if(new_users[i] == NULL) {
            break;
        }
        if(!isUserInList(new_users[i], users, USERS_TOTAL)) {
            redrawScreen = 1;
            users[USERS_TOTAL] = strdup(new_users[i]);
            USERS_TOTAL += 1;
            if(USERS_TOTAL > MAX_USERS_LENGTH) {
                USERS_TOTAL = 0;
            }
    /*        printf("After adding USERS_TOTAL NOW: %d ", USERS_TOTAL);
            for(int i = 0; i < USERS_TOTAL; i++) {
                printf(" user: %s ;;", users[i]);
            }
            printf("\n"); */
        }
    }


}
///////==========================================SERVER_START=====================================================//////////////


void get_users(int sock) {
    //setCursorPosition(0, TOTAL_HEIGHT + 15);
    int bytes_read;                                 //сколько байт прочитали
    char* message = "users";                        //название команды
    void* msg = malloc(strlen(message) + 4);        //переменная для отправки на сервер
    if (msg == NULL) {
        printf("Cant malloc memory");
        exit(2);
    }
    ((int*)msg)[0] = strlen(message);               //длина команды "users"
    for(int i = 0; i < strlen(message); i++) {
        ((char*)msg)[i+4] = message[i];             //копируем саму команду в переменную для отправки
    }
    arm_send(sock, msg, strlen(message) + 4, 0);


    //memset (test_buffer, 0, sizeof(test_buffer));
    bytes_read = arm_recv(sock, test_buffer, 6, 0); //сначала получим длину кол-ва и само кол-во(строкой). 4=длина, 2=байт на юзеров(считаем, что максимально 99, каждый символ по байту)

    //atoi дает sigfault, если не цифровой символ
    //int new_count = (int)(test_buffer+4);
    int new_count = atoi(test_buffer+4);
    char* new_users[MAX_USERS_LENGTH];

    ////USERS_TOTAL = atoi(test_buffer+4);              //пользователей у нас мало, поэтому число будет находиться в младшем байте
    //printf("Users(in chat)  bytes: %d\n", bytes_read);    //Длина сообщения, в котором переичслены сами пользователи
    //printf("Users in chat: %d\n", new_count); //Тут test_buffer[0] = 5, так как в начале число, а test_buffer[4] = количество пользователей в чате

    memset (test_buffer, 0, sizeof(test_buffer));   //Чтобы у последней строки гарантировано был нуль символ в конце
    for(int i = 0; i < new_count; i++) {
        bytes_read = arm_recv(sock, test_buffer, 4, 0); //длина очередного имени
        //printf("Another user length: %d\n", *(((int*)test_buffer)));
        bytes_read = arm_recv(sock, test_buffer, *(((int*)test_buffer)), 0); //имя
        new_users[i] = strdup(test_buffer);
        //printf("---%s\n", new_users[i]);
        memset (test_buffer, 0, sizeof(test_buffer));   //Чтобы не выводился предыдщуий пользователь вперемешку с текущим
    }
    updateUsers(new_users, new_count);
    free(msg);
}


void register_client(int socket) {
	//Формат(без плюсов и пробелов) len(command) + command + len(name) + name
    redrawScreen = 1;

	int bytes_read;
    int sock = socket;
    char* message = "register";
    void* msg = malloc(strlen(message) + 4);
    if (msg == NULL) {
        printf("Cant malloc memory");
        exit(2);
    }
    ((int*)msg)[0] = strlen(message);
    for(int i = 0; i < strlen(message); i++) {
        ((char*)msg)[i+4] = message[i];
    }
    arm_send(sock, msg, strlen(message)+4, 0);

    ((int*)msg)[0] = strlen(login);
    for(int i = 0; i < strlen(login); i++) {
        ((char*)msg)[4+i] = login[i];
    }
    arm_send(sock, msg, strlen(login)+4, 0);
    bytes_read =  arm_recv(sock, buffer, sizeof(buffer), 0);    //Берем с запасом sizeof(buffer), так как хз, что пришлет сервер

 //   printf("register command bytes: %d\n", bytes_read); //сколько пришло байт в ответе
 //   printf("register result: %s\n", buffer+4);                   //сам ответ заносим в доп инфу
    additionalInfo = strdup(buffer+4);
    free(msg);
}

void logout(int socket) {
    redrawScreen = 1;

	int bytes_read;
    int sock = socket;
    char* message = "logout";
    void* msg = malloc(strlen(message) + 4);
    if (msg == NULL) {
        printf("Cant malloc memory");
        exit(2);
    }
    ((int*)msg)[0] = strlen(message);
    for(int i = 0; i < strlen(message); i++) {
        ((char*)msg)[i+4] = message[i];
    }
    arm_send(sock, msg, strlen(message)+4, 0);
    bytes_read =  arm_recv(sock, buffer, sizeof(buffer), 0);
    printf("logout command bytes: %d\n", bytes_read); //сколько пришло байт в ответе
    printf("logout result: %s\n", buffer+4);                   //сам ответ
    additionalInfo = strdup(buffer+4);
    free(msg);
}

void send_message(int socket, char* receiver, char* context) {
    //TODO: перед отправкой проверять, присутствует ли юзер в чате. Если нет, то удалить отовсюду
    //PROTOCOL:
    //sock.send('send')
    //sock.send(message_to) - кому отправляем
    //sock.send(context) - что отправляем

    //отправляем команду
    redrawScreen = 1;

    int bytes_read;
    int sock = socket;
    char* message = "send";
    void* msg = malloc(strlen(message) + 4);
    if (msg == NULL) {
        printf("Cant malloc memory");
        exit(2);
    }
    if(strlen(context) < 1) {
        additionalInfo = "Please type some message";
    } else {
        ((int*)msg)[0] = strlen(message);
        for(int i = 0; i < strlen(message); i++) {
            ((char*)msg)[i+4] = message[i];
        }
        arm_send(sock, msg, strlen(message)+4, 0);

        //отправляем имя того, кому отправляем сообщение
        ((int*)msg)[0] = strlen(receiver);
        for(int i = 0; i < strlen(receiver); i++) {
            ((char*)msg)[i+4] = receiver[i];
        }
        arm_send(sock, msg, strlen(receiver)+4, 0);


        //отправляем сообщение
        ((int*)msg)[0] = strlen(context);
        for(int i = 0; i < strlen(context); i++) {
            ((char*)msg)[i+4] = context[i];
        }
        arm_send(sock, msg, strlen(context)+4, 0);
        free(msg);


        memset (test_buffer, 0, sizeof(test_buffer));
        bytes_read =  arm_recv(sock, buffer, sizeof(buffer), 0);
    //    printf("send command bytes: %d\n", bytes_read); //сколько пришло байт в ответе
    //    printf("send result: %s\n", buffer+4);                   //сам ответ
        additionalInfo = strdup(buffer+4);
        messageToSend = "";
    }
}

void receive_message(int socket) {
    /*Протокол:
    1)длина пакета, количество сооьщений(строкой)
    2)длина пакета, имя пользователя_1
    3)длина пакета, сообщение_1
    4)длина пакета, имя_пользователя_2
    5)длина пакета, сообщение_2
    ...
    */
    //redrawScreen = 1;

	int bytes_read;
    int sock = socket;
    int messages_count;
    char* message = "receive";
    //TODO не выделять лишний раз
    /*void* msg = malloc(strlen(message) + 4);
    msg = malloc(4+strlen(message)); */

    void* msg = malloc(strlen(message) + 4);

    if (msg == NULL) {
        printf("Cant malloc memory");
        exit(2);
    }
    ((int*)msg)[0] = strlen(message);
    for(int i = 0; i < strlen(message); i++) {
        ((char*)msg)[i+4] = message[i];
    }
    arm_send(sock, msg, strlen(message) + 4, 0);
    free(msg);


    memset (test_buffer, 0, sizeof(test_buffer));
    bytes_read = arm_recv(sock, test_buffer, 8, 0); //4 байта на длину + число в виде строки(вряд ли число=кол-ву сообщений будет больше 4 байт, поэтому 4+4=8)
    //atoi дает sigfault, если не цифровой символ
    //messages_count = (int)(test_buffer+4);
    messages_count = atoi(test_buffer+4);
    
//    printf("Messages(in total)  bytes: %d\n", bytes_read);    //Длина сообщения, в котором переичслены сами пользователи
//    printf("Messages(in total): %d\n", messages_count); //Тут test_buffer[0] = 5, так как в начале число, а test_buffer[4] = количество пользователей в чате

    /*if(messages_count < 1) {
        printf("No messages\n");
    }*/
//    printf("Messages list: \n");
    if(messages_count > 0) {
        redrawScreen = 1;
    }

    char* author;
    for(int i = 0; i < messages_count; i++) {
        memset (test_buffer, 0, sizeof(test_buffer));   //чтобы занулить прошлую информацию
        bytes_read = arm_recv(sock, test_buffer, 4, 0); //Длина имени автора
    //    printf("From:  user_length: %d;; ", *(((int*)test_buffer)));
        bytes_read = arm_recv(sock, test_buffer, *(((int*)test_buffer)), 0); //автор, не зануляем прошлую инфу, так как автор в ней
    //    printf("user: %s\n ___ ", test_buffer);
        author = strdup(test_buffer);

        memset (test_buffer, 0, sizeof(test_buffer));
        bytes_read = arm_recv(sock, test_buffer, 4, 0); //Длина сообщения
    //    printf(" message_length: %d;; ",*(((int*)test_buffer)));
        bytes_read = arm_recv(sock, test_buffer, *(((int*)test_buffer)), 0); //сообщение, не зануляем буфер по той же причине
    //    printf(" message: %s\n", test_buffer);


        messages[MESSAGESS_TOTAL] = strdup(test_buffer);
        authors[MESSAGESS_TOTAL] = strdup(author);
        MESSAGESS_TOTAL += 1;
        if(MESSAGESS_TOTAL > 40) {
            MESSAGESS_TOTAL = 0;
        }
    }
}

//TODO: TEST
void sendall_message(int sock, char* context) {
//    printf("Sending message to all\n");
    for(int i =0; i < USERS_TOTAL; i++) {
        if(users[i] != NULL) {
            send_message(sock, users[i], context);
            //receive_message(sock);
        } else {
           // printf("%s\n", users[i]);
            break;
        }
    }
}


void send_few_message(int sock, char** receivers, int rec_count, char* context) {
    //TODO: ВАЖНО!!! в receivers последний элемент должен быть NULL
//    printf("Sending message to few\n");
    for(int i = 0; i < rec_count; i++) {
        if(receivers[i] != NULL) {
            send_message(sock, receivers[i], context);
        } else {
            break;
        }
    }
}




///////==========================================SERVER_END=====================================================//////////////


//////=======================================START OF FRONTEND=================================================//////////////


void setCursorPosition(int XPos, int YPos) {
    printf("\033[%d;%dH", YPos+1, XPos+1);
}


/*void make_cursor_hide() {
    printf(HIDE_CURSOR, 90);
}

void make_cursor_show() {
    printf(SHOW_CURSOR, 90);
} */
int getch()
{
    struct termios oldattr, newattr;
    int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return ch;
}

/*int kbhit() {
    int ch;
    keyCode = getch();
    ch = keyCode;
    charPressed = keyCode;
    if(ch != EOF)
    {
        printf("You pressed: %d", ch);
        //ungetc(ch, stdin);
        return 1;
    }

    return 0;
} */
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();
    charPressed = ch;
    keyCode = (int)ch;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        //printf("You pressed: %d", ch);
        //ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

void switchUser(int command) {
    if(command == KEY_UP) {
        pointerToUser -= 1;
        if(pointerToUser < 0) {
            pointerToUser = USERS_TOTAL - 1;
        }
    } else if(command == KEY_DOWN) {
        pointerToUser += 1;
        if(pointerToUser == USERS_TOTAL) {
            pointerToUser = 0;
        }
    }
}

void switchMessage(int command) {
    if(command == KEY_UP) {
        pointerToMessage -= 1;
        if(pointerToMessage  < 0) {
            pointerToMessage  = MESSAGESS_TOTAL - 1;
        }
    } else if(command == KEY_DOWN) {
        pointerToMessage  += 1;
        if(pointerToMessage  == MESSAGESS_TOTAL) {
            pointerToMessage  = 0;
        }
    }

    if(pointerToMessage < 0) {
        pointerToMessage = 0;
    }
}

void switchWindow() {
    currentWindow += 1;
    currentWindow = currentWindow % WINDOWS_TOTAL;
}

int isUserChosen(char* username) {
    //return 0 если пользователь не выбран
    for(int i = 0; i < sizeof(chosenUsers); i++) {
        if(chosenUsers[i] == NULL) {
            return 0;
        } else {
            if (strcmp(username, chosenUsers[i]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

void deleteMessage() {  
    //Удаляет сообщение
    int j = 0;
    for(int i = pointerToMessage+1; i < MESSAGESS_TOTAL; i++) {
        if(messages[i] != NULL) {
            if(pointerToMessage == MESSAGESS_TOTAL - 1) {
                //messages[pointerToMessage+j] = NULL;
                //мы после цикла удаляем последний элемент так что норм
                j = pointerToMessage;
                break;
            } else  {
                messages[pointerToMessage+j] = messages[i];
                authors[pointerToMessage] = authors[i]; //с расчетом на то, что pointerToMessage указывает и на нужного автора
                j += 1;
                //i += 1;
            }
        } else {
            break;
        }
    }

    messages[MESSAGESS_TOTAL - 1] = NULL; //зануляем последнее сообщение, иначе будет дубликат
    authors[MESSAGESS_TOTAL - 1] = NULL;


    MESSAGESS_TOTAL -= 1;
    if(MESSAGESS_TOTAL < 0) {
        MESSAGESS_TOTAL = 0;
    }

    pointerToMessage -= 1;
    if(pointerToMessage < 0) {
        pointerToMessage = 0;
    }

/*    if(MESSAGESS_TOTAL == 0) {
        messages[0] = "test";
        messages[1] = "test2";
        messages[2] = "test3";
        messages[3] = "test4";
        messages[4] = "test5";
        messages[5] = "test6";
        messages[6] = "test7";
        messages[7] = "test8";
        messages[8] = "test9";
        messages[9] = "test10";
        messages[10] = "test11";
        messages[11] = "test12";
        messages[12] = "test13";
        messages[13] = "test14";
        MESSAGESS_TOTAL = 14;
    } */

//TODO: Прощальное сообщение - хз, расскоментировать ли или выводить только сообщение с сервера
/*  if(messages[0] == NULL) {
        messages[0] = "List of messages here";
        MESSAGESS_TOTAL += 1;
    } */
}

void deleteLetterMessage() {
    //TODO: добавить посимвольное стирание

    /*for(int i = 0; i < strlen(messageToSend); i++) {
        if(strlen(messageToSend) > 0) {
            messageToSend[strlen(messageToSend)-1] = NULL;
            break;
        }
    } */
    messageToSend = "";
}

void chooseUser() {
    int choosen = 0;
    int lastIndex = 0; //for adding new user
    int found_index = -1;
    for(int i = 0; i < chosenUsersCount; i++) {
        if(chosenUsers[i] != NULL) {
            if(strcmp(users[pointerToUser], chosenUsers[i]) == 0) {
                choosen = 1;
                found_index = i;
                break;
            }
        } else {
            lastIndex = i;
        }
    }
    if(choosen == 1) {
        //delete user from choosen
        int j = 0;
        for(int i = found_index + 1; chosenUsersCount; i++) {
            if(chosenUsers[i] != NULL) {
                chosenUsers[found_index+j] = chosenUsers[i];
                j += 1;
            } else {
                chosenUsers[i-1] = NULL;
                break;
            }
        }
        chosenUsersCount -= 1;
    } else {
        //if(lastIndex < sizeof(chosenUsers) && chosenUsersCount < 6) {
        if(chosenUsersCount < 6) {
            //add user to choosen
            if(chosenUsersCount == 0) {
                chosenUsers[0] = strdup(users[pointerToUser]);
            } else {
                for(int i = chosenUsersCount; i < 6; i++) {
                    if(chosenUsers[i] == NULL) {
                        chosenUsers[i] = strdup(users[pointerToUser]);
                        break;
                    }
                }
            }
            chosenUsersCount += 1;
        }
    }

    if(chosenUsersCount > 5) {
        additionalInfo = "You cant choose more than 6 users";
    }

}

void colorPrint(char* text, char* ansi_color) {
    printf("%s", ansi_color); //возможно из-за этого нету цвета
    printf("%s", text);
    printf(ANSI_COLOR_RESET);
}


void fill_positions(int msg_printed_length, int line_length) {
    if (msg_printed_length == 0) {
        printf("%s", bound);
        msg_printed_length += 1;
    }
    for(int k = 0; k < line_length - msg_printed_length - 1; k++) {
        printf("%s", delimeter);
    }
    printf("%s\n", bound);
}


void printMessageWindow() {
    //TODO: добавить прокрутку для случая, когда слишком много сообщений(== MSG_WINDOW_HEIGHT - 2) 
    setCursorPosition(0, 0);
    int printedMessages;
    int notPrint = 0;
    for(int i = 0; i < MESSAGE_WINDOW_HEIGHT; i ++) {
        //i - номер строки в окне
        //первая или последняя строка
        if(i == 0 || i == MESSAGE_WINDOW_HEIGHT-1) { 
            for(int j = 0; j < MESSAGE_WINDOW_LENGTH; j++) {
                printf("%s", bound);
            }
            printf("\n");
            //continue;
        } else {
            //везде в fill_positions отнимаем 1, так как в начале мы уже напечатали границу
            if(i-1 < MESSAGESS_TOTAL) {
                // i - 1, так как первая строка - это граница
                printf("%s", bound);

                int offset = 0;
                if(pointerToMessage >= MAX_MESSAGE_HEIGHT) {
                    offset = pointerToMessage; //если что удалить этот index отовсюду
                }

                int message_index = 0;
                if(i-1+offset < MESSAGESS_TOTAL) {
                    message_index = i-1+offset;
                } else {
                    message_index = offset;
                    notPrint = 1;
                }
                if(messages[message_index] != NULL && strcmp(messages[message_index], "") != 0) {

                    if(i + pointerToMessage > MESSAGESS_TOTAL) {
                        if(notPrint) {
                            printf("%s", bound);
                            fill_positions(1, MESSAGE_WINDOW_LENGTH);
                            continue;
                        }
                    }
                        //printf("Author:"); //For test
                        printf("%s:", authors[message_index]);
                        ////printf("%s",  messages[i-1]);
                        if(currentWindow == MESSAGES_WINDOW_NUM) {
                            if(pointerToMessage == message_index) {
                                colorPrint(messages[message_index], ANSI_COLOR_RED);
                            } else {
                                colorPrint(messages[message_index], ANSI_COLOR_GREEN);
                            }
                        } else {
                            printf("%s", messages[message_index]);
                        }
                        //fill_positions(strlen("Author:")+actual_strlen(messages[message_index]), MESSAGE_WINDOW_LENGTH);
                        //1 так как есть еще двоеточие
                        fill_positions(1+actual_strlen(authors[message_index])+actual_strlen(messages[message_index]), MESSAGE_WINDOW_LENGTH);
                } else {
                    //Если нет сообщений
                    fill_positions(1, MESSAGE_WINDOW_LENGTH);
                }

            } else {
                //Если мы ушли за пределы массива
                printf("%s", bound);
                fill_positions(1, MESSAGE_WINDOW_LENGTH);
            }
        }

    }

}

void printInputWindow() {
    //Это не ошибка, тут должно быть MESSAGE_WINDOW_HEIGHT
    setCursorPosition(0, MESSAGE_WINDOW_HEIGHT);
    for(int i = 0; i < INPUT_WINDOW_HEIGHT; i ++) {
        //i - номер строки в окне
        //первая или последняя строка
        if(i == 0 || i == INPUT_WINDOW_HEIGHT-1) {
            //fill_positions(0, INPUT_WINDOW_LENGTH); 
            for(int j = 0; j < INPUT_WINDOW_LENGTH; j++) {
                printf("%s", bound);
            }
            printf("\n");
        } else {
            //везде в fill_positions прибавляем 1, так как в начале мы уже напечатали границу
            int rowLen = 1;
            if(i == 1) {
                printf("%s", bound);
                //printf("To: ");
                if(currentWindow == INPUT_WINDOW_NUM) {
                    colorPrint("To: ", ANSI_COLOR_KCYN);
                } else {
                    printf("To: ");
                }
                rowLen += strlen("To: ");
                for(int k = 0; k < chosenUsersCount; k++) {
                    if(chosenUsers[k] != NULL) {
                        printf("%s, ", chosenUsers[k]);
                        rowLen += actual_strlen(chosenUsers[k]);
                        rowLen += 2; //запятая и пробел после 
                    }
                }
                fill_positions(rowLen-1, INPUT_WINDOW_LENGTH);
            } 
            else if(i == 2) { 
                printf("%s", bound);
                rowLen += 1;
                //printf("Message: %s", messageToSend);
                //greenPrint("Message: ");
                if(currentWindow == INPUT_WINDOW_NUM) {
                    colorPrint("Spravka: ", ANSI_COLOR_KCYN);
                } else {
                    //colorPrint("Info: ", ANSI_COLOR_MAGENTA);
                    printf("Spravka: ");
                }
                printf("%s", spravka);
                rowLen += strlen("Spravka: ");
                rowLen += actual_strlen(spravka);
                rowLen -= 2; // TODO хз почему, но работает
                fill_positions(rowLen, INPUT_WINDOW_LENGTH);            
            }
            else if(i == 3) {
                printf("%s", bound);
                rowLen += 1;
                //printf("Message: %s", messageToSend);
                //greenPrint("Message: ");
                if(currentWindow == INPUT_WINDOW_NUM) {
                    colorPrint("Info: ", ANSI_COLOR_KCYN);
                } else {
                    //colorPrint("Info: ", ANSI_COLOR_MAGENTA);
                    printf("Info: ");
                }
                printf("%s", additionalInfo);
                rowLen += strlen("Info: ");
                rowLen += actual_strlen(additionalInfo);
                rowLen -= 2; // TODO хз почему, но работает
                fill_positions(rowLen, INPUT_WINDOW_LENGTH);
            } else if(i == 4) {
                printf("%s", bound);
                rowLen += 1;
                //printf("Message: %s", messageToSend);
                //greenPrint("Message: ");
                if(currentWindow == INPUT_WINDOW_NUM) {
                    colorPrint("Message:", ANSI_COLOR_KCYN);
                } else {
                    //colorPrint("Message:", ANSI_COLOR_BLUE);
                    printf("Message:");
                }
                printf("%s", messageToSend);

                rowLen += strlen("Message:");
                rowLen += actual_strlen(messageToSend);
                rowLen -= 2; // TODO хз почему, но работает
                fill_positions(rowLen, INPUT_WINDOW_LENGTH);
            } else {
                fill_positions(0, INPUT_WINDOW_LENGTH+1);
            }



        /*  if(i-1 < (sizeof(messages) / 8)) {
                // i - 1, так как первая строка - это граница
                printf(bound);
                if(messages[i-1] != NULL) {
                    printf("Author:");
                    printf("%s",  messages[i-1]);
                    fill_positions(strlen("Author:")+strlen(messages[i-1]) + 1, MESSAGE_WINDOW_LENGTH);
                } else {
                    //Если нет сообщений
                    fill_positions(1, MESSAGE_WINDOW_LENGTH);
                }

            } else {
                //Если мы ушли за пределы массива
                fill_positions(1, MESSAGE_WINDOW_LENGTH);
            } */
        }

    }
}

void printPeopleWindow() {
    //TODO: перед этим проверить, не вышел ли кто-то и подкорректировать в зависимости от этого
    //TODO: добавить прокрутку для случая, когда слишком много пользователей (== TOTAL_HEIGHT - 2)
    //-1 так как мы сливаем границу с окном рядом
    setCursorPosition(MESSAGE_WINDOW_LENGTH, 0);
    int printedUsers = 0;
    int notPrint = 0;
    for(int i = 0; i < PEOPLE_WINDOW_HEIGHT; i++) {
        //i - номер строки в окне
        //первая или последняя строка
        //Это не ошибка, тут должно быть MESSAGE_WINDOW_LENGTH
        setCursorPosition(MESSAGE_WINDOW_LENGTH, i);
        if(i == 0 || i == PEOPLE_WINDOW_HEIGHT-1) { 
            for(int j = 0; j < PEOPLE_WINDOW_LENGTH; j++) {
                printf("%s", bound);
            }
            printf("\n");
            //continue;
        } else {
            //везде в fill_positions прибавляем к 1 аргументу 1, так как в начале мы уже напечатали границу
            //TODO: неправильный вариант: i-1 < (sizeof(users) / 8) ??
            if(i-1 < USERS_TOTAL) {
                // i - 1, так как первая строка - это граница
                printf("%s", bound);
                int additionalSymbol = 0;

                int offset = 0;
                if(pointerToUser >= MAX_PEOPLE_HEIGHT) {
                    offset = pointerToUser; //если что удалить этот index отовсюду
                }

                int user_index = 0;
                if(i-1+offset < USERS_TOTAL) {
                    user_index = i-1+offset;
                } else {
                    user_index = offset;
                    notPrint = 1;
                }

                
                if(users[user_index] != NULL) {
                    if(i + pointerToUser > USERS_TOTAL) {
                        if(notPrint) {
                            printf("%s", bound);
                            fill_positions(1, PEOPLE_WINDOW_LENGTH);
                            continue;
                        }
                    }
                    printedUsers += 1;
                    if(users[user_index] == users[pointerToUser]) {
                        printf(">");
                        additionalSymbol += 1;
                        if(currentWindow == PEOPLE_WINDOW_NUM) {
                            colorPrint(users[user_index], ANSI_COLOR_YELLOW);
                        } else {
                            printf("%s", users[user_index]);
                        }
                    } else if(isUserChosen(users[user_index]) == 1) {
                        if(currentWindow == PEOPLE_WINDOW_NUM) {
                            colorPrint(users[user_index], ANSI_COLOR_KCYN);
                        } else {
                            //colorPrint(users[i-1], ANSI_COLOR_RED);
                            printf("%s", users[user_index]);
                        }
                    } else {
                        printf("%s",  users[user_index]);
                    }

                    //Если сделать +1, то будет красивее, но только на первой странице
                    fill_positions(actual_strlen(users[user_index]) + additionalSymbol+1, PEOPLE_WINDOW_LENGTH);
                } else {
                    //Если чат пустой
                    fill_positions(1, PEOPLE_WINDOW_LENGTH);
                }

            } else {
                //Если мы ушли за пределы массива
                printf("%s", bound);
                fill_positions(1, PEOPLE_WINDOW_LENGTH);
            }
        }

    }
}

void keyboardHandler() {
    redrawScreen = 1;
    if (keyCode == ESC_CODE) {

        additionalInfo = "<<<<<<<GOOD BYE! SEE YOU SOON, MY FRIEND.>>>>>>>>";
        printInputWindow();
        printf(CLEAR_SCREEN);
      //  printf(SHOW_CURSOR);
        setCursorPosition(0, TOTAL_HEIGHT-1);
        printf("%s", additionalInfo);
        setCursorPosition(0, TOTAL_HEIGHT);
        logout(sock);
        close(sock);
        exit(0);
    } else {
        if(keyCode == TAB_CODE) {
            switchWindow();
        }
        if (currentWindow == PEOPLE_WINDOW_NUM) {
            if(keyCode == KEY_UP_CODE) {
                switchUser(KEY_UP);
            } else if (keyCode == KEY_DOWN_CODE) {
                switchUser(KEY_DOWN);
            } else if (keyCode == ENTER_CODE) {
                //switchUser(KEY_UP);
                chooseUser();
            }
        } else if (currentWindow == INPUT_WINDOW_NUM) {
            if(keyCode == BACKSPACE_CODE || keyCode == SECOND_BACKSPACE) {
                deleteLetterMessage();
            } else if(keyCode == ENTER_CODE) {
                //TODO: test
                //messageToSend; sock;
                if(chosenUsersCount < 1) {
                    additionalInfo = "Please choose user";
                } else {
                    send_few_message(sock, chosenUsers, chosenUsersCount, messageToSend);
                }
            } else if(keyCode == SEND_ALL_CODE) {
                if(actual_strlen(messageToSend) > 0 && messageToSend != NULL) {
                    sendall_message(sock, messageToSend);
                } else {
                    additionalInfo = "Please type some message";
                }
            } else {
                //TODO: с какого числа начинаются печатаемые символы?
                if(((int)keyCode) > 29) {
                    if(actual_strlen(messageToSend) < MSG_MAX_LENGTH) {
           

                        //UNCOMMENT
                        //size_t len = actual_strlen(messageToSend);
                        size_t len = strlen(messageToSend);
                        char addChar = '\0';

                        //int len = actual_strlen(messageToSend);
                        int add_mem = 2*sizeof(char);
                        if(keyCode == 0xd0 || keyCode == 0xd1) {
                            addChar = getchar();
                            add_mem += 1;
                        }
                        char* copy = malloc(len + add_mem);
                        if(copy == NULL) {
                            printf("CANT ALLOC MEMORY");
                            exit(2);
                        }
                        strcpy(copy, messageToSend);

                        //copy[len] = (char)keyCode;

                        copy[len] = charPressed;
                        if(addChar != '\0') {
                            copy[len+1] = addChar;
                            copy[len+2] = '\0';
                        } else {
                            copy[len+1] = '\0';
                        }
                        messageToSend = strdup(copy);
                        //^UNCOMMENT
                        //free(copy);
                        //free(toSend);


                        /*messageToSend = malloc(len + 2*sizeof(char)); //+2 для нового символа и нуль символа
                        if(messageToSend == NULL) {
                            printf("CANT ALLOC MEMORY");
                            exit(2);
                        }
                        strcpy(messageToSend, copy);
                        messageToSend[len] = (char*)keyCode;
                        messageToSend[len+1] = '\0';
                        free(copy); */
                    }
                }

            }
        } else if (currentWindow == MESSAGES_WINDOW_NUM) {
            if(keyCode == KEY_UP_CODE) {
                switchMessage(KEY_UP);
            } else if (keyCode == KEY_DOWN_CODE) {
                switchMessage(KEY_DOWN);
            } else if (keyCode == ENTER_CODE) {
                //switchUser(KEY_UP);
                deleteMessage();
            }

        }
    }
}



//////=======================================END OF FRONTEND=================================================//////////




int main() {
    if ((sock = arm_socket(ARM_AF_INET, ARM_SOCK_STREAM, 0)) < 0) {
        printf("Can't create socket!\n");
        exit(1);
    }
    struct arm_sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
    addr.sin_family = ARM_AF_INET;
    addr.sin_port = htons(PORT);
	inet_pton(ARM_AF_INET, IP, &addr.sin_addr);
    if (arm_connect(sock, (struct arm_sockaddr*) &addr, sizeof(addr)) < 0) {
        printf("Can't connect!\n");
        exit(2);
    }





    register_client(sock);

    //for test
/*    get_users(sock);
    //char* my_msg = "012345678901234567890123456789012345678901234567890123456789";
    char* my_msg = "йцуравыарымтавпокегкшуцгешщукцгешщукцопацпрагпцрмшгурпгурегу";
    send_message(sock, login, my_msg);
    my_msg = "another cool message";
    send_message(sock, login, my_msg);
    receive_message(sock);
    sendall_message(sock, "Hello to all users!");
    receive_message(sock);
    //тестируем пустое сообщение
    receive_message(sock);

    //for test
    printf("testing after server done; USERS: %d\n", USERS_TOTAL); */

  /*  while(1) {
        get_users(sock);
    } */

    //make_cursor_hide();
    int kod = 0;
    while(1) {
        setCursorPosition(0, TOTAL_HEIGHT+10);
        kod = kbhit();
        if(!kod) {
            get_users(sock);
            receive_message(sock);
            //setCursorPosition(0, TOTAL_HEIGHT+4); //TEST
            if(redrawScreen) {
                printf(CLEAR_SCREEN);
                printMessageWindow();
                printPeopleWindow();
                printInputWindow();
               /* setCursorPosition(0, TOTAL_HEIGHT);
                printf(" ");
                setCursorPosition(0, 0); */
                redrawScreen = 0;
                kod = 0;
            }
        } else {
          //  printf("TEST pressed key%d\n", keyCode);
          //  printf("TEST msg len: %d", strlen(messageToSend));
            keyboardHandler();
            /*setCursorPosition(0, TOTAL_HEIGHT+1);
            printf("                                  ");
            setCursorPosition(0, TOTAL_HEIGHT+4);
            printf("                                  "); */
        }
    }
    //make_cursor_show();
    //printf(SHOW_CURSOR);
    //printf("\nYOU pressed %c\n", getchar());
    printf("\n");
    printf("\n");
    //printf(chosenUsers[1]);
    return 0;

    logout(sock);
    close(sock);

    return 0;



}
