#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#define SNAKE_ARRAY_SIZE 310
#define MAX_SNAKES_NUM 50
#define MAX_SNAKE_SIZE 400
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>

volatile sig_atomic_t stop = 0;

typedef struct {
    int cursorX;//default position of the cursor
    int cursorY;
    int height;//map size
    int width;
    int ***snakes;//array of all snakes
    int **food;//array of all snakes
    int snakesNum;//number of snakes
    char* snakeChar;//array of snakes' charachters
    int* snakeSpeed;//array of snakes' speed
    int paused;
    int* currSnakeSize;
    pthread_mutex_t moveLock;
    pthread_mutex_t snakeInitLock;
} info;

//self-made gotoxy, which is not present in Linux gcc

void gotoxy(int x,int y)
{
    printf("%c[%d;%df",0x1B,y,x);
}

////////////////////////////////////////////////////////////////


//creating a map
void loadEnviroment(int consoleWidth, int consoleHeight, int X, int Y){
    //int i;
    int rectangleHeight = consoleHeight + 2;
    
    int x = X;
    int y = Y;
    gotoxy(x,y); //Top left corner
    
    for (; y < rectangleHeight; y++)
    {
        gotoxy(x, y); //Left Wall
        printf("%c",'|');
        
        gotoxy(consoleWidth + 2, y); //Right Wall
        printf("%c",'|');
    }
    
    y = Y;
    for (; x < consoleWidth + 3; x++)
    {
        gotoxy(x, y); //Left Wall
        printf("%c",'|');
        
        gotoxy(x, rectangleHeight); //Right Wall
        printf("%c",'|');
    }
    return;
}

//saving to a file
void savetoFile(int consoleWidth, int consoleHeight, FILE* f, info* gameInfo){
    //int i;
    
    //Creating an array which contanins the map and snakes
    char **snakeArr = (char **)malloc((consoleHeight+2) * sizeof(char *));
    for (int i=0; i<(consoleHeight+2); i++)
         snakeArr[i] = (char *)malloc((consoleWidth+2) * sizeof(char));
    
    for (int i = 0; i < (consoleHeight+2); i++)
        for (int j = 0; j < (consoleWidth+2); j++)
            snakeArr[i][j] = ' ';
    
    int rectangleHeight = consoleHeight + 1;
    
    int x = 0;
    int y = 0;
    //Adding map to the array
    for (; y < rectangleHeight; y++)
    {
        snakeArr[y][x] = '|';
        
        snakeArr[y][consoleWidth + 1] = '|'; //Right Wall
    }
    
    y = 0;
    for (; x < consoleWidth + 2; x++)
    {
        snakeArr[y][x] = '|';
        
        snakeArr[rectangleHeight][x] = '|';
    }
    //Adding snakes and food to the array
    for(int i = 0; i < gameInfo->snakesNum; i++){
        snakeArr[gameInfo->food[1][i]-1][gameInfo->food[0][i]-1] = 'o';
        for(int j = 0; j < gameInfo->currSnakeSize[i]; j++){
            if(gameInfo->snakes[i][1][j] > 0 && gameInfo->snakes[i][0][j] > 0){
                if(j == 0)
                    snakeArr[gameInfo->snakes[i][1][j]-1][gameInfo->snakes[i][0][j]-1] = gameInfo->snakeChar[i];
                else
                    snakeArr[gameInfo->snakes[i][1][j]-1][gameInfo->snakes[i][0][j]-1] = tolower(gameInfo->snakeChar[i]);
            }
        }
    }
    //Printing array to a file
    for (int i = 0; i < (consoleHeight+2); i++){
        for (int j = 0; j < (consoleWidth+2); j++)
            fprintf(f,"%c",snakeArr[i][j]);
        fprintf(f,"\n");
    }
    return;
}

//Checking if snake collides with itself or other snakes
int collisionCheck(int **snakeArr, int snakeSize, int direction, int mapHeight, int mapWidth, info *gameInfo){
    switch (direction) {
        case -1://Left
            for(int i = 0; i < gameInfo->snakesNum; i++){
                for(int j = 0; j < MAX_SNAKE_SIZE; j++){
                    if((snakeArr[0][0] - 1) == gameInfo->snakes[i][0][j] && snakeArr[1][0] == gameInfo->snakes[i][1][j])
                        return 1;
                }
            }
            return 0;
            
        case 1://Right
            for(int i = 0; i < gameInfo->snakesNum; i++){
                for(int j = 0; j < MAX_SNAKE_SIZE; j++){
                    if((snakeArr[0][0] + 1) == gameInfo->snakes[i][0][j] && snakeArr[1][0] == gameInfo->snakes[i][1][j])
                        return 1;
                }
            }
            return 0;
            
        case -2://Down
            for(int i = 0; i < gameInfo->snakesNum; i++){
                for(int j = 0; j < MAX_SNAKE_SIZE; j++){
                    if(snakeArr[0][0] == gameInfo->snakes[i][0][j] && (snakeArr[1][0] + 1) == gameInfo->snakes[i][1][j]){
                        return 1;
                    }
                }
            }
            return 0;
            
        case 2://Up
            for(int i = 0; i < gameInfo->snakesNum; i++){
                for(int j = 0; j < MAX_SNAKE_SIZE; j++){
                    if(snakeArr[0][0] == gameInfo->snakes[i][0][j] && (snakeArr[1][0] - 1) == gameInfo->snakes[i][1][j])
                        return 1;
                }
            }
            return 0;

    }
    return 0;
}

//Controller of snake movement
void moveSnake(int **snakeArr, int snakeSize, int direction, int mapHeight, int mapWidth, info *gameInfo, char schar, int speed){
    
    pthread_mutex_lock(&gameInfo->moveLock);
    //Checking if snake collides with anything
    if(collisionCheck(snakeArr,snakeSize,direction,mapHeight,mapWidth, gameInfo) == 1){
        pthread_mutex_unlock(&gameInfo->moveLock);
        return;
    }
    
    //Removing last element
    gotoxy(snakeArr[0][snakeSize-1],snakeArr[1][snakeSize-1]);
    printf("%c",' ');
    snakeArr[0][snakeSize-1] = -1;
    snakeArr[1][snakeSize-1] = -1;
    gotoxy(gameInfo->cursorX,gameInfo->cursorY);
    
    //Shifting array (i.e. 2nd element becomes 1st, 3rd becomes 2nd etc.)
    for(int i = snakeSize; i >= 1; i--){
        snakeArr[0][i] = snakeArr[0][i-1];
        snakeArr[1][i] = snakeArr[1][i-1];
    }
    //Replacing head element with tail element
    gotoxy(snakeArr[0][0],snakeArr[1][0]);
    printf("%c", tolower(schar));
    
    //Moving head to the chosen direction
    switch (direction) {
        case -1://Left
            snakeArr[0][0]--;
            break;
            
        case 1://Right
            snakeArr[0][0]++;
            break;
        
        case -2://Down
            snakeArr[1][0]++;
            break;
            
        case 2://Up
            snakeArr[1][0]--;
            break;
    }
    
    gotoxy(snakeArr[0][0],snakeArr[1][0]);
    printf("%c",schar);
    
    gotoxy(gameInfo->cursorX,gameInfo->cursorY);
    pthread_mutex_unlock(&gameInfo->moveLock);
    
    fflush(stdout);
    usleep(speed);
    return;
}

void firstStepCheck(int **snakeArr, int snakeSize, int direction, int mapHeight, int mapWidth, info *gameInfo, int schar, int speed){//Checking if snake overlaps itself after first step to a new target
    switch (direction) {
        case -1://Left
            if(snakeArr[0][0] > snakeArr[0][1]){
                if(snakeArr[1][0] == (mapHeight + 1)){
                    moveSnake(snakeArr,snakeSize,2,mapHeight,mapWidth,gameInfo,schar,speed);
                }else{
                    moveSnake(snakeArr,snakeSize,-2,mapHeight,mapWidth,gameInfo,schar,speed);
                }
            }
            break;
            
        case 1://Right
            if(snakeArr[0][0] < snakeArr[0][1]){
                if(snakeArr[1][0] == (mapHeight + 1)){
                    moveSnake(snakeArr,snakeSize,2,mapHeight,mapWidth,gameInfo,schar,speed);
                }else{
                    moveSnake(snakeArr,snakeSize,-2,mapHeight,mapWidth,gameInfo,schar,speed);
                }
            }
            break;
            
        case -2://Down
            if(snakeArr[1][0] < snakeArr[1][1]){
                if(snakeArr[0][0] == 2){
                    moveSnake(snakeArr,snakeSize,1,mapHeight,mapWidth,gameInfo,schar,speed);
                }else{
                    moveSnake(snakeArr,snakeSize,-1,mapHeight,mapWidth,gameInfo,schar,speed);
                }
            }
            break;
            
        case 2://Up
            if(snakeArr[1][0] > snakeArr[1][1]){
                if(snakeArr[0][0] == 2){
                    moveSnake(snakeArr,snakeSize,1,mapHeight,mapWidth,gameInfo,schar,speed);
                }else{
                    moveSnake(snakeArr,snakeSize,-1,mapHeight,mapWidth,gameInfo,schar,speed);
                }
            }
            break;
    }
}

void putSnake(int mapHeight, int mapWidth, int **snakeArr, char schar){
    //Creating a head
    int snakeX = rand() % (mapWidth + 1 - 2) + 2;
    int snakeY = rand() % (mapHeight + 1 - 2) + 2;
    gotoxy(snakeX,snakeY);
    snakeArr[0][0] = snakeX;
    snakeArr[1][0] = snakeY;
    printf("%c", schar);
    
    //And one char to a tail
    gotoxy(snakeX+1,snakeY);
    snakeArr[0][1] = snakeX + 1;
    snakeArr[1][1] = snakeY;
    printf("%c", tolower(schar));
}

void *createSnake(void *vargp){
    /*array of coordinates of snake's charachters
     It looks like that:
     
     x 0 1 4 5 6
     y 2 1 5 0 1
     
     where the first pair of coordinates is snake's head
     */
    int **snakeArr = (int **)malloc(2 * sizeof(int *));
    for (int i=0; i<2; i++)
         snakeArr[i] = (int *)malloc(400 * sizeof(int));
    
    info *gameInfo = vargp;
    //Getting all values
    pthread_mutex_lock(&gameInfo->snakeInitLock);
    int mapHeight = gameInfo->height;
    int mapWidth = gameInfo->width;
    char schar = gameInfo->snakeChar[gameInfo->snakesNum];
    int speed = gameInfo->snakeSpeed[gameInfo->snakesNum] * 1000;
    int snakeNumber = gameInfo->snakesNum;
    //And putting the values to shared variables
    gameInfo->snakes[snakeNumber] = snakeArr;
    gameInfo->snakesNum++;

    int snakeSize = 2;
    gameInfo->currSnakeSize[snakeNumber] = snakeSize;
    
    putSnake(mapHeight, mapWidth, snakeArr, schar);//Puts a new snake to a random point
    
    
    //Creating food
    int foodX = rand() % (mapWidth + 1 + 1 - 2) + 2;
    gameInfo->food[0][snakeNumber] = foodX;
    int foodY = rand() % (mapHeight + 1 + 1 - 2) + 2;
    gameInfo->food[1][snakeNumber] = foodY;
    gotoxy(foodX,foodY);
    printf("%c",'o');
    gotoxy(gameInfo->cursorX,gameInfo->cursorY);
    
    pthread_mutex_unlock(&gameInfo->snakeInitLock);
    
    int i = 0;
    //Snake's movement
    do {
        if(!gameInfo->paused){
            if(foodX > snakeArr[0][0]){
                if(i == 0)
                    firstStepCheck(snakeArr,snakeSize,1,mapHeight,mapWidth,gameInfo,schar,speed);
                moveSnake(snakeArr,snakeSize,1,mapHeight,mapWidth,gameInfo,schar,speed);
                
            }
            else if(foodX < snakeArr[0][0]){
                if(i == 0)
                    firstStepCheck(snakeArr,snakeSize,-1,mapHeight,mapWidth,gameInfo,schar,speed);
                moveSnake(snakeArr,snakeSize,-1,mapHeight,mapWidth,gameInfo,schar,speed);
            }
            else if(foodX == snakeArr[0][0]) {
                if (foodY < snakeArr[1][0]) {
                    if(i == 0)
                        firstStepCheck(snakeArr,snakeSize,2,mapHeight,mapWidth,gameInfo,schar,speed);
                    moveSnake(snakeArr,snakeSize,2,mapHeight,mapWidth,gameInfo,schar,speed);
                }else if(foodY > snakeArr[1][0]){
                    if(i == 0)
                        firstStepCheck(snakeArr,snakeSize,-2,mapHeight,mapWidth,gameInfo,schar,speed);
                    moveSnake(snakeArr,snakeSize,-2,mapHeight,mapWidth,gameInfo,schar,speed);
                }
                
            }
            
            
            i++;
            //Checking if snake ate food
            if(snakeArr[0][0] == foodX && snakeArr[1][0] == foodY){
                snakeSize++;
                gameInfo->currSnakeSize[snakeNumber] = snakeSize;
                //Creating food
                
                foodX = rand() % (mapWidth + 1 + 1 - 2) + 2;
                gameInfo->food[0][snakeNumber] = foodX;
                foodY = rand() % (mapHeight + 1 + 1 - 2) + 2;
                gameInfo->food[1][snakeNumber] = foodY;
                gotoxy(foodX,foodY);
                printf("%c",'o');
                gotoxy(gameInfo->cursorX,gameInfo->cursorY);
                
                i = 0;
            }
        }
    }while(1);
    
    return NULL;
}
//Allocating array, assigning all needed values and creating a map
info* prepareGame(int mapHeight, int mapWidth, int argNum, char** args, int argStartPos){
    
    info *gameInfo = malloc(sizeof *gameInfo);
    gameInfo->height = mapHeight;
    gameInfo->width = mapWidth;
    //Allocating an array of all snakes
    gameInfo->snakes = (int***)malloc((mapHeight*mapWidth/2) * sizeof(int**));
       for (int i = 0; i < (mapHeight*mapWidth/2); i++)
       {
           gameInfo->snakes[i] = (int**)malloc(2 * sizeof(int*));
           for (int j = 0; j < 2; j++)
           {
               gameInfo->snakes[i][j] = (int*)malloc(mapHeight*mapWidth * sizeof(int));
               }
           }
    //Allocating an array of food coordinates
    gameInfo->food = (int **)malloc(2 * sizeof(int *));
    for (int i=0; i<2; i++)
         gameInfo->food[i] = (int *)malloc(400 * sizeof(int));
    
    gameInfo->snakesNum = 0;
    gameInfo->paused = 0;
    gameInfo->cursorX = 1;
    gameInfo->cursorY = mapHeight + 3;
    gameInfo->snakeChar = (char*)malloc(sizeof(char)*mapHeight*mapWidth/2);
    gameInfo->snakeSpeed = (int*)malloc(sizeof(int)*mapHeight*mapWidth/2);
    gameInfo->currSnakeSize = (int*)malloc(sizeof(int)*mapHeight*mapWidth/2);

    for(int i = argStartPos; i < argNum; i++){
        gameInfo->snakeChar[i-argStartPos] = args[i][0];
 
        char *ptr = args[i];
        while (*ptr) {
            if ( isdigit(*ptr) ) {
                gameInfo->snakeSpeed[i-argStartPos] = (int)strtol(ptr,&ptr, 10);
            } else {
                ptr++;
            }
        }
    }
    system("clear"); //clear the console
    loadEnviroment(mapWidth, mapHeight, 1, 1);//Creating a map
    return gameInfo;
}

//Printing a map to a console
void printMap(info* gameInfo){
    
    loadEnviroment(gameInfo->width, gameInfo->height + gameInfo->cursorY + 1, 1, gameInfo->cursorY + 2);
    
    for(int i = 0; i < gameInfo->snakesNum; i++){
        gotoxy(gameInfo->food[0][i],gameInfo->food[1][i] + gameInfo->cursorY + 1);
        printf("%c",'o');
	int head = 0;
        for(int j = 0; j < gameInfo->currSnakeSize[i]; j++){
            gotoxy(gameInfo->snakes[i][0][j] ,gameInfo->snakes[i][1][j] + gameInfo->cursorY + 1);
            if(head == 0){
                printf("%c",toupper(gameInfo->snakeChar[i]));
		head = 1;
	    }else
                printf("%c",tolower(gameInfo->snakeChar[i]));
        }
    }
    gameInfo->cursorY += gameInfo->height + 4;
    gotoxy(1, gameInfo->cursorY);
}

void sigintHandler(int sig_num){
    stop = 1;
    return;
}

int main(int argc, char** argv)
{
    srand(time(NULL));
    
    //Handling SIGINT using sigaction in order to interrupt getchar() in the loop below
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sigintHandler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    int mapHeight = 10;
    int mapWidth = 40;
    char* saveFilePath = NULL;
    int i = 1;
    
   
    //Getting optional arguments
    int ch;
    opterr = 0;
    while ((ch = getopt (argc, argv, "x:y:f:")) != -1)
    switch (ch)
      {
      case 'x':
        mapWidth = atoi(optarg);
        i += 2;
        break;
      case 'y':
        mapHeight = atoi(optarg);
        i += 2;
        break;
      case 'f':
        saveFilePath = optarg;
        i += 2;
        break;
      case '?':
        if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        break;
      default:
        break;
      }
  
    info *gameInfo = prepareGame(mapHeight,mapWidth,argc,argv,i);//preparing game environment and allocating game information
    //Checking if path is still NULL after parsing arguments. If so, we get the path from environmental variable. If it is absent either, informing the user about it.
    if(saveFilePath == NULL){
        saveFilePath = getenv("SNAKEFILE");
        if(saveFilePath == NULL){
            printf("The game would not be saved!\n");
        }else{
            printf("\n");
        }
    }
    else{
        printf("\n");
    }
    printf("Press ENTER to start printing commands");
    
    pthread_t threads[10];
    //Initializing mutexes
    pthread_mutex_init(&(gameInfo->moveLock), NULL);
    pthread_mutex_init(&(gameInfo->snakeInitLock), NULL);
    //Creating threads
    for(int k = 0; k < argc - i; k++){
        pthread_create(&threads[k], NULL, createSnake, gameInfo);
    }
    
    //Command input handling
    char str[20] = {0};
    while(1){
        if(gameInfo->paused){//Checking if game is paused
            fgets(str,20,stdin);
            if(!strncmp(str, "exit", 4)){
                printf("\nGoodbye!\n");
                for(int i = 0; i < gameInfo->snakesNum; i++)
                    pthread_cancel(threads[i]);
                return 0;
            }//Spawn command
            else if(!strncmp(str, "spawn", 5)){
                gameInfo->snakeChar[gameInfo->snakesNum] = str[6];
                
                char *ptr = str;
                while (*ptr) {
                    if ( isdigit(*ptr) ) {
                        gameInfo->snakeSpeed[gameInfo->snakesNum] = (int)strtol(ptr,&ptr, 10);
                    } else {
                        ptr++;
                    }
                }
                pthread_create(&threads[gameInfo->snakesNum+1], NULL, createSnake, gameInfo);
            }//Show command
            else if(!strncmp(str, "show", 4)){
                printMap(gameInfo);
            }//Save command
            else if(!strncmp(str, "save", 4)){
                if(saveFilePath != NULL){
                    FILE *file = fopen(saveFilePath,"w");
                    savetoFile(mapWidth, mapHeight, file, gameInfo);
                    fclose(file);
                }
                    
            }
            gameInfo->cursorY += 2;
            printf("Press ENTER to start printing commands ");
            gameInfo->paused = 0;
        }else{
            char c = getchar();
            if(c=='\n'){
                gameInfo->paused = 1;
                printf(">");
            }else if(c==-1){//If SIGINT is called, getchar() returns -1;
                printf("\nGoodbye!\n");
                for(int i = 0; i < gameInfo->snakesNum; i++)
                    pthread_cancel(threads[i]);
                return 0;
            }
        }
    }
    
    
    //Joining threads
    for(int i = 0; i < gameInfo->snakesNum + 1; i++)
        pthread_join(threads[i], NULL);
    
    
  return(0);
}
