//**********************************************************************//
//                                                                      //
//                   Atividade 2 / FINAL                                //
//     IOT11 - Sistemas Operacionais de Tempo Real                      //
//     Aluno: Alessandro Ribeiro Lage                                   //
//     Data: 17/08/2022                                                 //
//                                                                      //
//**********************************************************************//
//                                                                      //
//   Estamos vivendo tempos tenebrosos, com grandes animosidades        // 
//   entre as lideranças mundiais, polarização política, quebra de      //
//   paradigmas e tabus sociais... Em resumo, um caldeirão de           //
//   pólvora!                                                           //
//   Alta tecnologia é sempre bom, mas o simples pode ser a única       //
//   saída em momentos de crise. Por isso, desenvolva uma solução com   //
//   o FreeRTOS que faça a transformação de texto em código morse,      //
//   representado por ‘.’ e ‘- ́. Também deverá ser reproduzido de        //
//   forma visual, através da representação de LED, usado nos exemplos. //
//   A aplicação deverá ser composta por pelo menos 4 tasks, sendo:     //
//   • Leitura de teclado;                                              //
//   • Processador de texto, codificando a frase sempre que for         //
//     identificado a tecla enter;                                      //
//   • Codificador para morse em caracteres ‘.’ e ‘-’;                  //
//   • Reprodutor visual do código morse gerado.                        //
//   A troca de dados entre as tasks deverão acontecer através          //
//   de QUEUEs.                                                         //
//                                                                      //
//**********************************************************************//


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"
#include <termios.h>

//Definição das prioridades de cada Task
#define TASK1_PRIORITY 1 //Prioridade da tarefa 1
#define TASK2_PRIORITY 1 //Prioridade da tarefa 2
#define TASK3_PRIORITY 1 //Prioridade da tarefa 3
#define TASK4_PRIORITY 3 //Prioridade da tarefa 4


//Definições do programa
#define BLACK "\033[30m" /* Black */
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */
#define WHITE "\033[37m" /* White */
#define DISABLE_CURSOR() printf("\e[?25l")
#define ENABLE_CURSOR() printf("\e[?25h")
#define MAX_TEXT_SIZE 100  //define o tamanho máximo do texto a ser codificado em Morse

#define clear() printf("\033[H\033[J")
#define gotoxy(x, y) printf("\033[%d;%dH", (y), (x))


int linha_inicial=7; //tamanho do texto digitado pelo usuário - tarefa 3
int x=0; //contadores auxiliares da task_1 
int cta=0; //conta texto - tarefa 3
int flag_task3 = 1; //flag para suspender a task 3
char texto_vetor[MAX_TEXT_SIZE]; //armazena o texto recebido antes de ser codificado em Morse

typedef struct
{
    int pos;
    char *color;
    int period_ms;
} st_led_param_t;

st_led_param_t white = {
    5,   // Define a posição onde o LED vai aparecer
    WHITE,  // Define a cor do LED
    100}; // Define o estado de acesso do LED vermelho deve durar 100ms

TaskHandle_t Task1_hdl, Task2_hdl, Task3_hdl, Task4_hdl;
QueueHandle_t structQueue_1_to_2 = NULL;
QueueHandle_t structQueue_2_to_3 = NULL;
QueueHandle_t structQueue_3_to_4 = NULL;

//Task 1- Leitura do teclado e processamento do texto e envio para a Queue
static void Task_leitura_teclado (void *pvParameters)
{
    char texto; //variável interna que recebe a tecla digitada no teclado

    /* I need to change  the keyboard behavior to
    enable nonblock getchar */
    struct termios initial_settings,
        new_settings;

    tcgetattr(0, &initial_settings);

    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 1;

    tcsetattr(0, TCSANOW, &new_settings);
    /* End of keyboard configuration */

    
    for (;;)
    {
       if (flag_task3 == 1) // flag = 1 indica que a task está ativa e pode ser suspensa (a própria task ativa o flag quando termina a tarefa princiapal)
       {
         flag_task3 = 0;  //flag = 0 indica que a task está suspensa
         vTaskSuspend(Task3_hdl);
       }
       else
       {
          texto = getchar();   //leitura do teclado
          if (texto >= 1 && texto <= 126) //filtra carateres indesejados
          {
             
             if (xQueueSendToBack(structQueue_1_to_2, &texto, 10) == pdTRUE) // passar dados para tarefa 2 via queue
             {
               cta++; //conta numero de caracteres enviados, inclusive o ENTER
               gotoxy(cta,linha_inicial);
               texto=toupper(texto);
               putchar(texto);
               fflush(stdout);
               vTaskResume(Task2_hdl); // chama a Task para processamento do texto enviado
             }
          }
       }

    }
    vTaskDelay(10/portTICK_PERIOD_MS);
    tcsetattr(0, TCSANOW, &initial_settings);
    ENABLE_CURSOR();
    exit(0);
    vTaskDelete(NULL);
}

//Task 2 - Processador de texto
//Recebe a leitura do teclado da task 1 e processa (filtra e encontra o caracter ENTER com fim da mensagem)
static void Task_processador_texto (void *pvParameters)
{
   char texto_received_from_1;
   char texto_envia;
   for (;;)
   {
      if (xQueueReceive(structQueue_1_to_2, &texto_received_from_1, 0) == pdPASS)
      { // a if abaixo filtra somente letras do alfabeto, numeros de 0 a 9, a tecla Espaço e ENTER.
         if ((texto_received_from_1 >= 48 && texto_received_from_1 <= 57) || (texto_received_from_1 >= 65 && texto_received_from_1 <= 90) || (texto_received_from_1 >= 97 && texto_received_from_1 <= 122) || texto_received_from_1 == 32|| texto_received_from_1 == '\n')
         {
            texto_envia = toupper(texto_received_from_1);  //converte para maiúscula para diminuir a pesquisa no swith case
            if (xQueueSendToBack(structQueue_2_to_3, &texto_envia, 10) == pdTRUE) // passar dados para tarefa 2 via queue
            {
               //gotoxy(cta,linha_inicial);
               //putchar(texto_envia); // imprime o texto na tela como referência para o usuário do que está sendo digitado antes de ser enviado com ENTER
               fflush(stdout);
            }
         }
         if (texto_received_from_1 == '\n') // Quando detectado a tecla ENTER termina de salvar o vetor e incia o envio para conversão em Morse
         {
            vTaskResume(Task3_hdl); // chama a Task para conversão do texto em código Morse
            //vTaskSuspend(Task1_hdl); //Suspende a Task 1 para não ler o teclado até terminar o processamento
            vTaskSuspend(Task2_hdl); //Suspende a Task 2 para não enviar mais dados na fila
         }
      }
      vTaskDelay(100/ portTICK_PERIOD_MS); // delay da task
   }
}

//Task 3 - Converte o texto recebido pela Queue em código Morse
//A tarefa armazerna o texto recebido em um vetor e faz a codificação quando recebe o caracter ENTER.

//Função parte da Task 3 - Para envio de dados da tarefa 3 para a tarefa 4 por uma Queue
static void transmite_sinal_para_led(char *sinal)
{

    if (xQueueSend(structQueue_3_to_4, &sinal, 100) != pdTRUE)
    {
      printf("Erro na fila 3!");
      exit(1);
    }
}

static void Task_codificador_morse(void *pvParameters)
{
   char texto_received_from_2; // texto a ser recebido de outra tarefa
   char texto_envia;
   for (;;)
   {
      if (xQueueReceive(structQueue_2_to_3, &texto_received_from_2, 100) == pdPASS)
      {
         switch (texto_received_from_2)
         {
         case 48:               // 0
            printf("-----   "); // três espaços em branco no final diferencia espaço entre caracteres - Observação para todos caracteres
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            break;
         case 49: // 1
            printf(".----   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 50: // 2
            printf("..---   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 51: // 3
            printf("...--   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 52: // 4
            printf("....-   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 53: // 5
            printf(".....   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 54: // 6
            printf("-....   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 55: // 7
            printf("--...   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 56: // 8
            printf("---..   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 57: // 9
            printf("----.   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 32:           // Espaço
            printf("    "); // sete espaços em branco diferencia espaço entre palavras - foi colocado 4 porque depois de cada letra já tem 3
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 65: // A
            printf(".-   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 66: // B
            printf("-...   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 67: // C
            printf("-.-.   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 68: // D
            printf("-..   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 69: // E
            printf(".   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 70: // F
            printf("..-.   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 71: // G
            printf("--.  ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 72: // H
            printf("....   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 73: // I
            printf("..   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 74: // J
            printf(".---   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 75: // K
            printf("-.-   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 76: // L
            printf(".-..   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 77: // M
            printf("--   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 78: // N
            printf("-.   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 79: // O
            printf("---   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 80: // P
            printf(".--.   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 81: // Q
            printf("--.-   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 82: // R
            printf(".-.   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 83: // S
            printf("...   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 84: // T
            printf("-   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 85: // U
            printf("..-   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 86: // V
            printf("...-   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 87: // W
            printf(".--   ");
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 88: // X
            printf("-..-   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 89: // Y
            printf("-.---   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case 90: // Z
            printf("--..   ");
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'-');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)'.');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            transmite_sinal_para_led((char *)' ');
            fflush(stdout);
            break;
         case '\n': // Enter
            transmite_sinal_para_led((char *)'\n');
            linha_inicial=linha_inicial+3;
            // printf("li=%d",linha_inicial);
            if(linha_inicial==13) //apaga duas linhas a frente
            {
               gotoxy(0,7);
               printf("                                                                                       \n");
               printf("                                                                                       \n");
               fflush(stdout);
               linha_inicial = 13;
            }
            if (linha_inicial == 16)
            {
               gotoxy(0,10);
               printf("                                                                                       \n");
               printf("                                                                                       \n");
               fflush(stdout);
               linha_inicial = 7;
            }
            if (linha_inicial == 10)
            {
               gotoxy(0,13);
               printf("                                                                                       \n");
               printf("                                                                                       \n");
               fflush(stdout);
               linha_inicial = 10;
            }
            flag_task3 = 1;
            cta=0;
            //vTaskResume(Task1_hdl); //Retorma a tarefa de leitura do teclado, pois já fez todo o processamento
            vTaskSuspend(Task3_hdl);
            vTaskSuspend(Task2_hdl);
            break;
         }
      }
   vTaskDelay(100 / portTICK_PERIOD_MS); // delay da task
   }
}

static void Task_morse_led (void *pvParameters)
{
    // pvParameters contains LED params
    st_led_param_t *led = (st_led_param_t *)pvParameters;
    portTickType xLastWakeTime = xTaskGetTickCount();
    char texto_recebido;

    for (;;)
    {
        if (xQueueReceive(structQueue_3_to_4, &texto_recebido, pdMS_TO_TICKS(10000)) == pdPASS)
        {
            
            if(texto_recebido=='.') //ponto
            {
                gotoxy(led->pos, 4);
                printf("%s⬤\n", led->color);
                //printf("%c\n",texto_recebido);
                fflush(stdout);              
                vTaskDelay(100 / portTICK_PERIOD_MS);

            }

            if(texto_recebido=='-') //traço
            {
                gotoxy(led->pos, 4);
                printf("%s⬤", led->color);
                //printf("%c\n",texto_recebido);
                fflush(stdout);
                vTaskDelay(300 / portTICK_PERIOD_MS);
            }

            if (texto_recebido == ' ')//espaço
            {
               gotoxy(led->pos, 4);
               printf("%s ", BLACK);
               fflush(stdout);
               vTaskDelay(300 / portTICK_PERIOD_MS);
            }

            if (texto_recebido == '\n')//Enter
            {
               vTaskResume(Task1_hdl);
               vTaskResume(Task2_hdl);
               vTaskResume(Task3_hdl);
               vTaskSuspend(NULL);
            }

            gotoxy(led->pos, 4);
            printf("%s ", BLACK);
            fflush(stdout); 
            //vTaskSuspend(NULL);

        }
        else{

            printf("Erro de recebimento de fila, tarefa do led!", led->color);
            vTaskSuspend(NULL);
        }
    }

    vTaskDelete(NULL);
}

void app_run(void)
{
   structQueue_1_to_2 = xQueueCreate(100, // Queue length
                              2); // Queue item size

   structQueue_2_to_3 = xQueueCreate(100, // Queue length
                              2);  // Queue item size

   structQueue_3_to_4 = xQueueCreate(1000, // Queue length
                                     2);  // Queue item size
   clear();
   printf("Programa iniciado... Digite o texo a ser codificado e pressione ENTER!\n\n");

   if (structQueue_1_to_2 == NULL)
   {
       printf("Falha ao criar a Queue 1. Programa abortado!n");  //Imprime erro se houver falha ao criar a Queue
       exit(1);
   }

      if (structQueue_2_to_3 == NULL)
   {
       printf("Falha ao criar a Queue 2. Programa abortado!n");  //Imprime erro se houver falha ao criar a Queue
       exit(1);
   }
         if (structQueue_3_to_4 == NULL)
   {
       printf("Falha ao criar a Queue 3. Programa abortado!n");  //Imprime erro se houver falha ao criar a Queue
       exit(1);
   }

   DISABLE_CURSOR();
   printf(
   "╔════════╗\n"
   "║        ║\n"
   "╚════════╝\n");

   xTaskCreate(Task_leitura_teclado, "Ler_Teclado", configMINIMAL_STACK_SIZE, NULL, TASK1_PRIORITY, &Task1_hdl);         // Task1 = Leitura dos dados do teclado
   xTaskCreate(Task_processador_texto, "Processa_Texto", configMINIMAL_STACK_SIZE, NULL, TASK2_PRIORITY, &Task2_hdl);    // Task2 = Processador de texto
   xTaskCreate(Task_codificador_morse, "Codificador_Morse", configMINIMAL_STACK_SIZE, NULL, TASK3_PRIORITY, &Task3_hdl); // Task3 = Reproduz texto em código morse na tela
   //xTaskCreate(Task_morse_led, "Morse_Led", configMINIMAL_STACK_SIZE, &white, TASK4_PRIORITY, &Task4_hdl);                 // Task4 = Reproduz texto em código morse no Led

   /* Start the tasks and timer running. */
   vTaskStartScheduler();
   vTaskSuspend(Task2_hdl);
   vTaskSuspend(Task3_hdl);
   //vTaskSuspend(Task4_hdl);

   /* If all is well, the scheduler will now be running, and the following
    * line will never be reached.  If the following line does execute, then
    * there was insufficient FreeRTOS heap memory available for the idle and/or
    * timer tasks      to be created.  See the memory management section on the
    * FreeRTOS web site for more details. */
   for (;;)
   {
      printf("Erro de memória!\n");
    }
}