/*
 *  Lab 4: Baccarat
 *  Carter King
 *  Operating Systems
 *  Due 4/4/19
 *
 *
 *
 *  This program creates two child threads, known as the Player and the Bank, while the main() acts as the Croupier in the game of Baccarat.
 *  Using pthread mutex locks and condition variables, the game has the players draw their hands then determine if a winner has 
 *  been decided, or if there is a need to draw a third card to decide a winner. The program keeps a tally of the Player wins, Bank wins,
 *  and ties to determine a final winner. 
 *
 *  sources used:
 *  https://moodle.rhodes.edu/pluginfile.php/266641/mod_resource/content/1/threads-cv.pdf
 *  http://www.cplusplus.com/reference/cstdlib/srand/
 *  https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032r/index.html
 */


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#define deckSize 52
#define maxHand 3
//discern within data structure who you are working with
#define PLAYER 0
#define BANKER 1

//  array of all possible cards
int deck [deckSize] = {2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6,6,6,6,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,10,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14};
pthread_cond_t p = PTHREAD_COND_INITIALIZER;
pthread_cond_t b = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;


enum validity {FALSE, TRUE};

//validity of commands from croupier
enum validity valid[2] = {FALSE, FALSE};

//actions: GO = draw initial hand, 
//DRAW= draw third card
//STOP= chill until given further instruction
//END= game is completely over, close thread
enum action{GO, DRAW, STOP, END};

//commands from croupier to bank/player
enum action cmd[2]= {STOP, STOP};

//condition variable to make sure p and b hands are valid
int hValid[2] = {0,0};

//value of the hands 
int hValue[2] = {-1,-1};

//how many cards have been received by each 
int cardsDealt[2] = {0,0};

//hold the cards in the hand
int pHand[3] = {-1,-1,-1};
int bHand[3] = {-1,-1,-1};

//booleans to ease process
enum validity needToDraw = FALSE;
enum validity gameOver = FALSE;

//keep tally of results
int pWins = 0;
int bWins = 0;
int ties = 0;


int drawCard();
void * banker(void * arg);
void * player(void * arg);
int calcValue(int addend1, int addend2);
char * calcCard(int card);
void goLogic();
void drawLogic();


/*
 * drawCard(): function draws a single card from deck for player and bank
 *  using a random index integer
 */


int drawCard(){
  int randIndex = rand() % deckSize;
  return randIndex;
}


/*
 * banker(): This function handles the banker child thread in acting upon
 *  the necessary moves for the banker in the game of Baccarat
 */


void * banker(void * arg){
  do{
    //lock the action
    pthread_mutex_lock(&m);
    
    //wait if Banker hasn't received valid cmd from croupier
    while(valid[BANKER] == FALSE){
      pthread_cond_wait(&b, &m);
    }
    
    //if croupier wants banker to draw initial hand
    if(cmd[BANKER] == GO){
      //chang validity of croupier cmd
      valid[BANKER] = FALSE;
      //BCard1
      bHand[0] = deck[drawCard()];
      cardsDealt[BANKER] += 1;
      //BCard2
      bHand[1] = deck[drawCard()];
      cardsDealt[BANKER] += 1;
      //Value of Hand
      hValue[BANKER] = calcValue(bHand[0], bHand[1]);
      //hValid should be equal to 1 to know that initial hand is completed
      hValid[BANKER] ++;
      //signal crouper that GO is complete
      pthread_cond_signal(&b);
    }
    
    //if croupier wants Banker to DRAW
    if(cmd[BANKER] == DRAW){
      //invalidate command
      valid[BANKER] = FALSE;
      //BCard3
      bHand[2] = deck[drawCard()];
      cardsDealt[BANKER] += 1;
      hValue[BANKER] = calcValue(hValue[BANKER], bHand[2]);
      //banker hand is valid after drawing card
      hValid[BANKER] ++;
      pthread_cond_signal(&b);
    }
    pthread_mutex_unlock(&m);
  
  //END tells banker to stop looping as complete game is over  
  }while(cmd[BANKER] != END);
  return NULL;
}


/*
 * player(): this function handles the player thread to enact the necessary
 *  moves for the game of baccarat.
 */


void * player(void * arg){
  do{
    //lock the action
    pthread_mutex_lock(&m);
    
    //wait until croupier has sent valid cmd
    while(valid[PLAYER] == FALSE){
      pthread_cond_wait(&p, &m);
    }
    
    //croupier wants player to draw initial hand
    if(cmd[PLAYER] == GO){
      //invalidate cmd
      valid[PLAYER] = FALSE;
      //pCard1
      pHand[0] = deck[drawCard()];
      cardsDealt[PLAYER] += 1;
      //pCard2
      pHand[1] = deck[drawCard()];
      cardsDealt[PLAYER] += 1;
      //value of Hand
      hValue[PLAYER] = calcValue(pHand[0], pHand[1]);
      //player initial hand is valid
      hValid[PLAYER] ++;
      pthread_cond_signal(&p);
    }
    
    //croupier needs player to draw a third card
    if(cmd[PLAYER] == DRAW){
      //invalidate cmd
      valid[PLAYER] = FALSE;
      //PCard3
      pHand[2] = deck[drawCard()];
      cardsDealt[PLAYER] += 1;
      hValue[PLAYER] = calcValue(hValue[PLAYER], pHand[2]);
      //player hand is valid
      hValid[PLAYER] ++;
      pthread_cond_signal(&p);
    }
    
    pthread_mutex_unlock(&m);
  }while(cmd[PLAYER] != END);
  return NULL;
}


/*
 * calcValue(): Takes two card values and returns the proper sum based on the
 *  scenario in baccarat.
 */


int calcValue(int addend1, int addend2){
  int finValue;
  int val1 = addend1;
  int val2 = addend2;
  int sum;

  if (addend1 >=10){
    val1 = 0;
  }

  if (addend2 >= 10){
    val2 = 0;
  }

  sum = val1 + val2;
  if(sum >= 10){
    finValue = sum % 10;
  }
  else{
    finValue = sum;
  }

  return finValue;
}


/*
 *  calcCards(): translate card value into name of card
 *    ex: 11 == Jack
 */


char * calcCard(int card){
  char * cardName;
  switch(card){
    case 2:
      cardName = "2";
      break;
    case 3:
      cardName = "3";
      break;
    case 4:
      cardName = "4";
      break;
    case 5:
      cardName = "5";
      break;
    case 6:
      cardName = "6";
      break;
    case 7:
      cardName = "7";
      break;
    case 8:
      cardName = "8";
      break;
    case 9:
      cardName = "9";
      break;
    case 10:
      cardName = "10";
      break;
    case 11:
      cardName = "J";
      break;
    case 12:
      cardName = "Q";
      break;
    case 13:
      cardName = "K";
      break;
    case 14:
      cardName = "A";
      break;
  }
  return cardName;
}


/*
 * goLogic(): handles the evaluation of the initial hands and decides if the game is not decided
 *  immediately if the player needs to draw a third or stand
 */


void goLogic(){
  
  //Check if round ends in a tie
  if((hValue[PLAYER] == 8 || hValue[PLAYER] == 9) && (hValue[BANKER] == 8 || hValue[BANKER] == 9)){
    printf("Round ends in a Tie!\n");
    gameOver = TRUE;
    ties ++;
  }
  
  //Check if player wins outright
  if((hValue[PLAYER] == 8 || hValue[PLAYER] == 9) && (hValue[BANKER] < 8)){
    printf("Player wins\n");
    gameOver = TRUE;
    pWins ++;
  }
  
  //Check if banker wins outright
  if((hValue[BANKER] == 8 || hValue[BANKER] == 9) && (hValue[PLAYER] < 8)){
    printf("Bank wins\n");
    gameOver = TRUE;
    bWins ++;
  }
  
  if(gameOver == FALSE){
    
    //Does player need to draw a Third Card?
    if(hValue[PLAYER] >=0 && hValue[PLAYER]<= 5){
      valid[PLAYER] = TRUE;
      cmd[PLAYER] = DRAW;
      pthread_cond_signal(&p);
    }
    
    else{
      //Player doesn't need to draw a third card
      printf("Player Stands\n");
      valid[PLAYER] = TRUE;
      cmd[PLAYER] = STOP;
      pthread_cond_signal(&p);
      //let it be known player's hand has been evaluated and is valid
      hValid[PLAYER] ++;
    }
  }
}


/*
 * drawLogic(): This function handles the logic for the game of baccarat once the banker needs to decide
 *  how to act after the player has stood or drawn a third card.
 */


void drawLogic(){
  setbuf(stdout, NULL);
  
  //Before Banker can think about drawing, need to know that player hand is completely valid,
  // aka hValid[PLAYER] == 2
  while(hValid[PLAYER] != 2){
    pthread_cond_wait(&p, &m);
  }
  
  //If player drew third card, print it out, bc "player stand" was handled in goLogic
  if(cardsDealt[PLAYER] == 3){
    printf("Player draws %s\n", calcCard(pHand[2]));
  }
  
  //if Banker needs to draw a third card
  if((hValue[BANKER] >= 0 && hValue[BANKER] <= 2) ||
     (hValue[BANKER] == 3 && pHand[2] != 8) ||
     (hValue[BANKER] == 4 && (pHand[2] >= 2 && pHand[2] <= 7)) ||
     (hValue[BANKER] == 5 && (pHand[2] >= 4 && pHand[2] <= 7)) ||
     (hValue[BANKER] == 6 && (pHand[2] == 6 || pHand[2] == 7))) {
    
    valid[BANKER] = TRUE;
    cmd[BANKER] = DRAW;
    pthread_cond_signal(&b);
     
    //needs to be hValid to be 2  to know that banker has finished drawing third card
    while(hValid[BANKER] != 2){
     pthread_cond_wait(&b, &m);
    }
    printf("Bank draws %s\n", calcCard(bHand[2]));
    valid[BANKER] = TRUE;
    cmd[BANKER] = STOP;
    pthread_cond_signal(&b);
  }
  
  //Banker stands with initial value
  else{
    printf("Bank Stands\n");
    valid[BANKER] = TRUE;
    cmd[BANKER] = STOP;
    pthread_cond_signal(&b);
    hValid[BANKER] ++;
  }
  
  //determine the winner
  if(hValue[BANKER] > hValue[PLAYER]){
    printf("Bank wins\n");
    gameOver = TRUE;
    bWins ++;
  }
  
  else if(hValue[PLAYER] > hValue[BANKER]){
    printf("Player wins\n");
    gameOver = TRUE;
    pWins ++;
  }
  
  else{
    printf("Tie\n");
    gameOver = TRUE;
    ties ++;
  }
}


/*
 *  main(): acts as the croupier parent thread in the baccarat game by
 *    determining the winner.
 */


int main(int argc, char **argv) {
  
  if (argc != 2){
    perror("parameter issue");
    exit(1);
  
  }
  int createRet;
  setbuf(stdout, NULL);
  
  //determine how many rounds of baccarat to play
  int numRounds = atoi(argv[1]);

  pthread_t bankT;
  pthread_t playerT;
  
  //set random seed to use for card drawing
  srand(time(NULL));
  
  //Create thread for player and banker
  createRet = pthread_create(&bankT, NULL, banker, NULL);
  if (createRet != 0){
    perror("thread create");
    exit(1);
  }
  createRet = pthread_create(&playerT, NULL, player, NULL);
  if (createRet != 0){
    perror("thread create");
    exit(1);
  }

  printf("Beginning %d Rounds. . .\n", numRounds);
  
  //loop the game the number of rounds supplied
  for(int x = 0; x < numRounds; x++){
    printf("------------------------\n");
    printf("Beginning Round %d\n", x + 1);
    pthread_mutex_lock(&m);
    
    //Give player and bank the go ahead to draw their initial hand
    // by setting proper variables then signaling
    valid[PLAYER] = TRUE;
    valid[BANKER] = TRUE;
    cmd[PLAYER] = GO;
    cmd[BANKER] = GO;
    pthread_cond_signal(&p);
    pthread_cond_signal(&b);
    
    //wait until player has properly drawn initial hand
    while(hValid[PLAYER] != 1){
      pthread_cond_wait(&p, &m);
    }
    
    //wait until banker has properly drawn initial hand
    while(hValid[BANKER] != 1){
      pthread_cond_wait(&b, &m);
    }
    
    //print results of initial hand
    printf("Player draws %s, %s\n", calcCard(pHand[0]), calcCard(pHand[1]));
    printf("Bank draws %s, %s\n", calcCard(bHand[0]), calcCard(bHand[1]));
    
    //perform baccarat logic on the result of the initial hands
    goLogic();
    
    //if game is not over after inital draw, run further logic
    if(gameOver == FALSE){
      drawLogic();
    }
    
    //winner of round determined, reset all necessary shared variables
    for(int y = 0; y < 2; y++){
      valid[y] = FALSE;
      cmd[y] = STOP;
      hValid[y] = 0;
      hValue[y] = -1;
      cardsDealt[y] = 0;
    }
    
    for(int z =0; z < 3; z++){
      pHand[z] = -1;
      bHand[z] = -1;
    }
    needToDraw = FALSE;
    gameOver = FALSE;
    pthread_mutex_unlock(&m);
  }
  
  //Entire game is over, send player/banker END cmds
  pthread_mutex_lock(&m);
  cmd[PLAYER] = END;
  valid[PLAYER] = TRUE;
  cmd[BANKER] = END;
  valid[BANKER] = TRUE;
  pthread_cond_signal(&p);
  pthread_cond_signal(&b);
  pthread_mutex_unlock(&m);

  //print final tallies
  printf("----------------------\n");
  printf("Results:\n");
  printf("Player: %d\n", pWins);
  printf("Bank: %d\n", bWins);
  printf("Ties: %d\n", ties);
  if(bWins > pWins){
    printf("Bank wins!\n");
  }
  else if(pWins > bWins){
    printf("Player wins!\n");
  }
  else{
    printf("TIE!\n");
  }
  
  //Join threads back to parent
  (void) pthread_join(bankT, NULL);
  (void) pthread_join(playerT, NULL);

  return 0;
}	
