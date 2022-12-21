//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.
#include <iostream>
#include <string>
#include <random>
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <thread>
//
#include "gl_frontEnd.h"
//
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

//	feel free to "un-use" std if this is against your beliefs.
using namespace std;
pthread_t t1;
//==================================================================================
//	Function prototypes
//==================================================================================
void initializeApplication(void);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = Direction::NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
void generatePartitions(void);
void* singleThreadFunc(void* args);
void* threadFunc(void* args);
bool checkNextSquare(struct TravelerToPass *localTraveler, Direction currentDir);
void pathFinding(struct TravelerToPass *localTraveler);

//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
unsigned int numRows = 0;	//	height of the grid
unsigned int numCols = 0;	//	width
unsigned int numTravelers = 0;	//	initial number
unsigned int numTravelersDone = 0;
unsigned int movesBeforeGrowth = 0;		//  Number of moves before the traveler grows
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition	exitPos;	//	location of the exit
vector<thread> threadList;

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t launchTime;

//	Random generators:  For uniform distributions
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 6;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, static_cast<unsigned int>(Direction::NUM_DIRECTIONS)-1);
uniform_int_distribution<unsigned int> headsOrTails(0, 1);
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;


//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void updatePos(struct Traveler * localTraveler){
	for(int i = localTraveler->numberOfSegments; i > 0; i--){
		localTraveler->segmentList[i].col=localTraveler->segmentList[i-1].col;
		localTraveler->segmentList[i].row=localTraveler->segmentList[i-1].row;
		localTraveler->segmentList[i].dir=localTraveler->segmentList[i-1].dir;

	}
}


void moveTravelerN(struct Traveler * localTraveler){
	updatePos(localTraveler);
	localTraveler->segmentList[0].dir = Direction::NORTH;
	localTraveler->segmentList[0].row--;
}


void moveTravelerS(struct Traveler * localTraveler){
	updatePos(localTraveler);
	localTraveler->segmentList[0].dir = Direction::SOUTH;
	localTraveler->segmentList[0].row++;
}


void moveTravelerE(struct Traveler * localTraveler){
	updatePos(localTraveler);
	localTraveler->segmentList[0].dir = Direction::EAST;
	localTraveler->segmentList[0].col++;


}

void moveTravelerW(struct Traveler * localTraveler){
	updatePos(localTraveler);
	localTraveler->segmentList[0].dir = Direction::WEST;
	localTraveler->segmentList[0].col--;

}

void drawTravelers(void)
{
	//-----------------------------
	//	You may have to sychronize things here
	//-----------------------------
	for (unsigned int k=0; k<travelerList.size(); k++)
	{
		//	here I would test if the traveler thread is still live
		drawTraveler(travelerList[k]);
		//drawTraveler()
	}
}

void updateMessages(void)
{
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 4;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "I like cheese");
	sprintf(message[3], "Simulation run time: %ld s", time(NULL)-launchTime);
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;


	switch (c)
	{
		//	'esc' to quit
		case 27:
			exit(0);
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
		//pthread
/*		158-162 moves traveler			*/				

			//travelerToPass.travelersPassed =&travelerList;
			//travelerToPass.travelerIdx = 0;
			//travelerToPass.directionOfHead =(char *)"East";
			//moveTraveler(travelerToPass);
			moveTravelerN(&travelerList[0]);

			speedupTravelers();
			ok = 1;
			//travelerList = travelerToPass.travelersPassed;
			break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}



bool canMove(struct Traveler *currentTraveler){
	unsigned int rowPos = currentTraveler->segmentList[0].row;
	unsigned int colPos = currentTraveler->segmentList[0].col;
	
	if(grid[rowPos - 1][colPos] == SquareType::FREE_SQUARE && rowPos - 1 > 0 ){
		return true;
	}
	else if (grid[rowPos + 1][colPos] == SquareType::FREE_SQUARE && rowPos + 1 < numRows){
		return true;
	}
	else if(grid[rowPos][colPos - 1] == SquareType::FREE_SQUARE && colPos - 1 > 0){
		return true;
	}
	else if(grid[rowPos][colPos + 1] == SquareType::FREE_SQUARE && colPos + 1 < numCols){
		return true;
	}
	return false;
}



void move(struct Traveler *currentTraveler){
	unsigned int rowPos = currentTraveler->segmentList[0].row;
	unsigned int colPos = currentTraveler->segmentList[0].col;
	vector<Direction> possibleMoves;

	//TRAVERER OF SIZE 1 handeling
	if(grid[rowPos - 1][colPos] == SquareType::FREE_SQUARE && rowPos - 1 > 0 ){
		possibleMoves.push_back(Direction::NORTH);
	}
	if (grid[rowPos + 1][colPos] == SquareType::FREE_SQUARE && rowPos + 1 < numRows){
		possibleMoves.push_back(Direction::SOUTH);
	}
	if(grid[rowPos][colPos - 1] == SquareType::FREE_SQUARE && colPos - 1 > 0){
		possibleMoves.push_back(Direction::WEST);
	}
	if(grid[rowPos][colPos + 1] == SquareType::FREE_SQUARE && colPos + 1 < numCols){
		possibleMoves.push_back(Direction::EAST);
	}
	srand(time(0));

	for(int i = currentTraveler->numberOfSegments; i > 0; i--){
		currentTraveler->segmentList[i].col = currentTraveler->segmentList[i-1].col;
		currentTraveler->segmentList[i].row = currentTraveler->segmentList[i-1].row;
		currentTraveler->segmentList[i].dir = currentTraveler->segmentList[i-1].dir;
	}

	currentTraveler->segmentList[0].dir = possibleMoves[rand() % possibleMoves.size()];

	if (currentTraveler->segmentList[0].dir == Direction::EAST){
		currentTraveler->segmentList[0].col++;
	}
	else if (currentTraveler->segmentList[0].dir == Direction::WEST){
		currentTraveler->segmentList[0].col--;
	}
	else if(currentTraveler->segmentList[0].dir == Direction::SOUTH){
		currentTraveler->segmentList[0].row++;
	}
	else if(currentTraveler->segmentList[0].dir == Direction::NORTH){
		currentTraveler->segmentList[0].row--;
	}
	printf("Traveler %d:    Row: %d      Col: %d\n", currentTraveler->index ,currentTraveler->segmentList[0].row, currentTraveler->segmentList[0].col);
}


void* travelerThreadFunc(void* args){
	Traveler *currentTraveler = (Traveler *)args;

	bool keepMoving = true;
	unsigned int rowPos, colPos;

	while(keepMoving != false){
		rowPos = currentTraveler->segmentList[0].row;
		colPos = currentTraveler->segmentList[0].col;

		if(rowPos == exitPos.row && colPos == exitPos.col){
			keepMoving = false;
		}
		else if(canMove(currentTraveler) == false){
			keepMoving = false;
		}
		else{
			move(currentTraveler);
			usleep(travelerSleepTime);
		}
	}
	return 0;
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%.  No upper limit on sleep time.
	//	We can slow everything down to admistrative pace if we want.
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}




void newMove(int travPos){
	unsigned int rowPos = travelerList[travPos].segmentList[0].row;
	unsigned int colPos = travelerList[travPos].segmentList[0].col;
	vector<Direction> possibleMoves;

	//TRAVERER OF SIZE 1 handeling
	if(grid[rowPos - 1][colPos] == SquareType::FREE_SQUARE && rowPos - 1 > 0 ){
		possibleMoves.push_back(Direction::NORTH);
	}
	if (grid[rowPos + 1][colPos] == SquareType::FREE_SQUARE && rowPos + 1 < numRows){
		possibleMoves.push_back(Direction::SOUTH);
	}
	if(grid[rowPos][colPos - 1] == SquareType::FREE_SQUARE && colPos - 1 > 0){
		possibleMoves.push_back(Direction::WEST);
	}
	if(grid[rowPos][colPos + 1] == SquareType::FREE_SQUARE && colPos + 1 < numCols){
		possibleMoves.push_back(Direction::EAST);
	}
	srand(time(0));

	for(int i = travelerList[travPos].numberOfSegments; i > 0; i--){
		travelerList[travPos].segmentList[i].col = travelerList[travPos].segmentList[i-1].col;
		travelerList[travPos].segmentList[i].row = travelerList[travPos].segmentList[i-1].row;
		travelerList[travPos].segmentList[i].dir = travelerList[travPos].segmentList[i-1].dir;
	}

	travelerList[travPos].segmentList[0].dir = possibleMoves[rand() % possibleMoves.size()];

	if (travelerList[travPos].segmentList[0].dir == Direction::EAST){
		travelerList[travPos].segmentList[0].col++;
	}
	else if (travelerList[travPos].segmentList[0].dir == Direction::WEST){
		travelerList[travPos].segmentList[0].col--;
	}
	else if(travelerList[travPos].segmentList[0].dir == Direction::SOUTH){
		travelerList[travPos].segmentList[0].row++;
	}
	else if(travelerList[travPos].segmentList[0].dir == Direction::NORTH){
		travelerList[travPos].segmentList[0].row--;
	}
}


bool newCanMove(int travPos){
	unsigned int rowPos = travelerList[travPos].segmentList[0].row;
	unsigned int colPos = travelerList[travPos].segmentList[0].col;
	
	if(grid[rowPos - 1][colPos] == SquareType::FREE_SQUARE && rowPos - 1 > 0 ){
		return true;
	}
	else if (grid[rowPos + 1][colPos] == SquareType::FREE_SQUARE && rowPos + 1 < numRows){
		return true;
	}
	else if(grid[rowPos][colPos - 1] == SquareType::FREE_SQUARE && colPos - 1 > 0){
		return true;
	}
	else if(grid[rowPos][colPos + 1] == SquareType::FREE_SQUARE && colPos + 1 < numCols){
		return true;
	}
	return false;
}






void newThreadFunc(int travPos){
	
	bool keepMoving = true;
	unsigned int rowPos, colPos;

	while(keepMoving != false){
		rowPos = travelerList[travPos].segmentList[0].row;
		colPos = travelerList[travPos].segmentList[0].col;

		if(rowPos == exitPos.row && colPos == exitPos.col){
			keepMoving = false;
		}
		else if(newCanMove(travPos) == false){
			keepMoving = false;
		}
		else{
			newMove(travPos);
			usleep(travelerSleepTime);
		}
		printf("Traveler %d:    Row: %d      Col: %d\n", travPos ,travelerList[travPos].segmentList[0].row, 
		travelerList[travPos].segmentList[0].col);
	}
}





//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard code-some values
	numRows = atoi(argv[1]);
	numCols = atoi(argv[2]);
	numTravelers = atoi(argv[3]);
	movesBeforeGrowth = atoi(argv[4]);
	numLiveThreads = 0;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);
	
	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);
	
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i=0; i< numRows; i++)
		free(grid[i]);
	free(grid);
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);
	
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================


void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		for (unsigned int j=0; j< numCols; j++)
			grid[i][j] = SquareType::FREE_SQUARE;
		
	}

	message = new char*[MAX_NUM_MESSAGES];
	for (unsigned int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color
	srand((unsigned int) time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = SquareType::EXIT;

	//	Generate walls and partitions
	generateWalls();
	generatePartitions();

	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float** travelerColor = createTravelerColors(numTravelers);
	for (unsigned int k=0; k<numTravelers; k++) {
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.segmentList.push_back(seg);
		grid[pos.row][pos.col] = SquareType::TRAVELER;

		//	I add 0-n segments to my travelers
		unsigned int numAddSegments = segmentNumberGenerator(engine);
		TravelerSegment currSeg = traveler.segmentList[0];
		bool canAddSegment = true;
		//cout << "Traveler " << k << " at (row=" << pos.row << ", col=" <<
		//pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
		//cout << "\t";

		traveler.numberOfSegments = numAddSegments;

		for (unsigned int s=0; s<numAddSegments && canAddSegment; s++)
		{
			TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
			if (canAddSegment)
			{
				traveler.segmentList.push_back(newSeg);
				currSeg = newSeg;
				//cout << dirStr(newSeg.dir) << "  ";
			}
		}
		//cout << endl;

		for (unsigned int c=0; c<4; c++)
			traveler.rgba[c] = travelerColor[k][c];
		
		travelerList.push_back(traveler);
	}

	//int passToThread = 0;

	//thread firstThread(newThreadFunc, passToThread);


	//firstThread.join();
	/*
	int p = fork();

	if (p==0){
		
		pthread_t thread_id;
		
		pthread_create(&thread_id, NULL, &travelerThreadFunc, &travelerList[0]);

		bool notDone = true;
		while (notDone == true){
			usleep(travelerSleepTime);
		}
		
	
	}

	
	//SINGLE TRAVELER
	pthread_t thread_id;

	struct TravelerToPass singleTraveler;
	singleTraveler.travelersPassed = &travelerList;
	singleTraveler.travelerIdx = 0;
	singleTraveler.directionOfHead = (char *)"East";
	

	pthread_create(&thread_id, NULL, &singleThreadFunc, &singleTraveler);

	pthread_join(thread_id, NULL);

	//MULTIPLE TRAVELERS
	TravelerToPass travelers[numTravelers];
	pthread_t th[numTravelers];

	for (unsigned int i = 0; i < numTravelers; i++){
		struct TravelerToPass currentTraveler;
		currentTraveler.travelersPassed = &travelerList;
		currentTraveler.travelerIdx = i;
		currentTraveler.directionOfHead = (char *)"East";
		travelers[i] = currentTraveler;
	}

	for (unsigned int j = 0; j < numTravelers; j++){
		pthread_create(&th[j], NULL, &threadFunc, &travelers[j]);
	}

	//Wait for threads to finish
	for(unsigned int i = 0; i < numTravelers; i++){
		pthread_join(th[i], NULL);
		numTravelersDone ++;
		
	}
	*/

	
	
	//	free array of colors
	for (unsigned int k=0; k<numTravelers; k++)
		delete []travelerColor[k];
	delete []travelerColor;
	
}


void* singleThreadFunc(void* args){
	
	TravelerToPass *localTraveler = (TravelerToPass *)args; 
	bool goalReached = false;
	unsigned int currentRow, currentCol;
	int count = 0;
	
	//printf("Enter Thread Func\n");

	while(goalReached != true){
		currentRow = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].row;
		currentCol = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].col;
		
		//printf("%d \n", count);

		if(currentRow == exitPos.row && currentCol == exitPos.col){
			goalReached = true;
		}
		if(currentRow > exitPos.row){
			localTraveler->directionOfHead =(char *)"North";
			moveTraveler(localTraveler);
			usleep(travelerSleepTime);
		}
		else if (currentRow < exitPos.row){
			localTraveler->directionOfHead =(char *)"South";
			moveTraveler(localTraveler);
			usleep(travelerSleepTime);
		}
		//Check east/west movement
		if(currentCol > exitPos.col){
			localTraveler->directionOfHead =(char *)"West";
			moveTraveler(localTraveler);
			usleep(travelerSleepTime);
		}
		else if (currentCol < exitPos.col){
			localTraveler->directionOfHead =(char *)"East";
			moveTraveler(localTraveler);
			usleep(travelerSleepTime);
		}
		count ++;
	}
	//printf("Single Thread Func Complete\n");
	return 0;
}

//THREAD FUNC
//NOTES: KEEP CHECKING IF THERE IS A WALL IN FIRST DIRECTION IF NOT TRAVELING TOWARDS GOAL
//MAKE SURE TO CHECK WALLS
//
//WHILE (Not Finished){
//  WHILE(TRaveling towards goal) 
//	 If(Not Blocked){ moveTraveler() }
//   Else if (Wall) {pathFinding() }
//
//}
void* threadFunc(void* args){
	TravelerToPass *localTraveler = (TravelerToPass *)args; 

	bool goalReached = false;
	bool movingToGoal, colMove, rowMove, isNextFree;
	unsigned int currentRow, currentCol;
	Direction currentDir, colDir, rowDir;
	
	while(goalReached != true){
		currentRow = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].row;
		currentCol = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].col;
		currentDir = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].dir;
		
		
		if(currentRow == exitPos.row && currentCol == exitPos.col){
			goalReached = true;
		}

		//Check north/south movement
		if(currentRow > exitPos.row){
			rowDir = Direction::NORTH;
			rowMove = true;
		}
		else if (currentRow < exitPos.row){
			rowDir = Direction::SOUTH;
			rowMove = true;
		}
		else{
			rowMove = false;
			rowDir = Direction::WEST;
		}
		//Check east/west movement
		if(currentCol > exitPos.col){
			colDir = Direction::WEST;
			colMove = true;
		}
		else if (currentCol < exitPos.col){
			colDir = Direction::EAST;
			colMove = true;
		}
		else{
			colMove = false;
			colDir = Direction::NORTH;
		}
		
		//Check to see if Traveler is moving towards the goal
		if(currentDir == colDir && colMove == true){
			movingToGoal = true;
		}
		else if(currentDir == rowDir && rowMove == true){
			movingToGoal = true;
		}
		
		
		//Check for wall then move if traveler is moving in correct direction
		if(movingToGoal == true){
			isNextFree = checkNextSquare(localTraveler, currentDir);
			if(isNextFree == true){
				moveTraveler(localTraveler);
			}
		}
		else{
			if(checkNextSquare(localTraveler, rowDir) == true){
				//Change the direction of the traveler's head and move the traveler
				localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].dir = rowDir;
				moveTraveler(localTraveler);
			}
			else if(checkNextSquare(localTraveler, colDir) == true){
				//Change the direction of the traveler's head and move the traveler
				localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].dir = colDir;
				moveTraveler(localTraveler);
			}
			else if(checkNextSquare(localTraveler, currentDir) == true){
				//Move the traveler
				moveTraveler(localTraveler);
			}
			else{
				pathFinding(localTraveler);
			}
		}
	}
	//printf("Multithread Complete\n");
	return 0;
}

bool checkNextSquare(struct TravelerToPass *localTraveler, Direction currentDir){
	unsigned int currentRow = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].row;
	unsigned int currentCol = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].col;

	if (currentDir == Direction::EAST){
		if(grid[currentRow][currentCol + 1] == SquareType::FREE_SQUARE){
			return true;
		}
		else{
			return false;
		}
	}
	else if(currentDir == Direction::WEST){
		if(grid[currentRow][currentCol - 1] == SquareType::FREE_SQUARE){
			return true;
		}
		else{
			return false;
		}
	}
	else if(currentDir == Direction::NORTH){
		if(grid[currentRow + 1][currentCol] == SquareType::FREE_SQUARE){
			return true;
		}
		else{
			return false;
		}
	}
	else{
		if(grid[currentRow - 1][currentCol] == SquareType::FREE_SQUARE){
			return true;
		}
		else{
			return false;
		}
	}
}


//Use struct
//Get the traveler to goal so give it a direction
void pathFinding(struct TravelerToPass *localTraveler){
	unsigned int currentRow = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].row;
	unsigned int currentCol = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].col;
	Direction currentDir = localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].dir;

	if(currentDir == Direction::EAST || currentDir == Direction::WEST){
		//Check to see which direction is towards goal
		if(currentRow > exitPos.row){
			//Check to see if tile is open
			if(grid[currentRow + 1][currentCol] == SquareType::FREE_SQUARE){
				currentDir = Direction::NORTH;
			}
			else{
				currentDir = Direction::SOUTH;
			}
		}
		else{
			//Check to see if tile is open
			if(grid[currentRow - 1][currentCol] == SquareType::FREE_SQUARE){
				currentDir = Direction::SOUTH;
			}
			else{
				currentDir = Direction::NORTH;
			}
		}
	}
	else{
		//Check to see which direction is towards goal
		if(currentCol > exitPos.col){
			//Check to see if tile is open
			if(grid[currentRow][currentCol + 1] == SquareType::FREE_SQUARE){
				currentDir = Direction::WEST;
			}
			else{
				currentDir = Direction::EAST;
			}
		}
		else{
			//Check to see if tile is open
			if(grid[currentRow][currentCol - 1] == SquareType::FREE_SQUARE){
				currentDir = Direction::EAST;
			}
			else{
				currentDir = Direction::WEST;
			}
		}
	}
	localTraveler->travelersPassed[0][localTraveler->travelerIdx].segmentList[0].dir = currentDir;
}


//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = Direction::NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir==forbiddenDir);
	}
	return dir;
}


TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
		case Direction::NORTH:
			if (	currentSeg.row < numRows-1 &&
					grid[currentSeg.row+1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row+1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::SOUTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::SOUTH:
			if (	currentSeg.row > 0 &&
					grid[currentSeg.row-1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row-1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::NORTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::WEST:
			if (	currentSeg.col < numCols-1 &&
					grid[currentSeg.row][currentSeg.col+1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col+1;
				newSeg.dir = newDirection(Direction::EAST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::EAST:
			if (	currentSeg.col > 0 &&
					grid[currentSeg.row][currentSeg.col-1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col-1;
				newSeg.dir = newDirection(Direction::WEST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;
		
		default:
			canAdd = false;
	}
	
	return newSeg;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols+numRows)/4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;
	
	//	Generate the vertical walls
	for (unsigned int w=0; w< NUM_WALLS; w++)
	{
		goodWall = false;
		
		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_WALLS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_WALLS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
	}
}


void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		goodPart = false;
		
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_PARTS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*HSP + HSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
				}
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_PARTS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*VSP + VSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
				}
			}
		}
	}
}
