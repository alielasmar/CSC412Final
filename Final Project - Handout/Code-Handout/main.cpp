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
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
//	feel free to "un-use" std if this is against your beliefs.
using namespace std;


mutex mutexLock;


//==================================================================================
//	Function prototypes
//==================================================================================
void initializeApplication(void);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = Direction::NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
void generatePartitions(void);
void singleThreadFunc(struct Traveler *localTraveler);
void moveTraveler(struct Traveler *localTraveler);
void moveDirection(struct Traveler *localTraveler, Direction currentDir);
void moveDirectionParition(struct Traveler *localTraveler, Direction currentDir);

//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
mutex*** gridLocks;
unsigned int numRows = 0;	//	height of the grid
unsigned int numCols = 0;	//	width
unsigned int numTravelers = 0;	//	initial number
unsigned int numTravelersDone = 0;
unsigned int movesBeforeGrowth = 0;		//  Number of moves before the traveler grows
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList; 
GridPosition	exitPos;	//	location of the exit
vector<thread*> threadList; //  list of pointers to threads

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
bool appRunning = true;   //  keeps track of whether app should keep running

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

void erasePartition(SlidingPartition * localPartition, Direction directionMoving){
	// erases full partition to enable sliding
	int blockListSize = localPartition->blockList.size()-1;

	if(directionMoving == Direction::EAST){
		if(grid [localPartition->blockList[0].row] [localPartition->blockList[0].col-1] == SquareType::HORIZONTAL_PARTITION){
			gridLocks[localPartition->blockList[0].row] [localPartition->blockList[0].col-1]->lock();
		grid [localPartition->blockList[0].row] [localPartition->blockList[0].col-1] = SquareType::FREE_SQUARE;
			gridLocks[localPartition->blockList[0].row] [localPartition->blockList[0].col-1]->unlock();
		}
	}
	if(directionMoving == Direction::WEST){
		if(grid [localPartition->blockList[blockListSize].row] [localPartition->blockList[blockListSize].col+1] == SquareType::HORIZONTAL_PARTITION){
		gridLocks[localPartition->blockList[blockListSize].row] [localPartition->blockList[blockListSize].col+1]->lock();
		grid [localPartition->blockList[blockListSize].row] [localPartition->blockList[blockListSize].col+1] = SquareType::FREE_SQUARE;
		gridLocks[localPartition->blockList[blockListSize].row] [localPartition->blockList[blockListSize].col+1]->unlock();
		}
	}
	if(directionMoving == Direction::SOUTH){
		if(grid [localPartition->blockList[0].row-1] [localPartition->blockList[0].col] == SquareType::VERTICAL_PARTITION){
		gridLocks [localPartition->blockList[0].row-1] [localPartition->blockList[0].col]->lock();
		grid [localPartition->blockList[0].row-1] [localPartition->blockList[0].col] = SquareType::FREE_SQUARE;
		gridLocks [localPartition->blockList[0].row-1] [localPartition->blockList[0].col]->unlock();
		}
	}
	if(directionMoving == Direction::NORTH){
		if(grid [localPartition->blockList[blockListSize].row+1] [localPartition->blockList[blockListSize].col] == SquareType::VERTICAL_PARTITION){
		gridLocks  [localPartition->blockList[blockListSize].row-1] [localPartition->blockList[blockListSize].col]->lock();
		grid [localPartition->blockList[blockListSize].row-1] [localPartition->blockList[blockListSize].col] = SquareType::FREE_SQUARE;
		gridLocks  [localPartition->blockList[blockListSize].row-1] [localPartition->blockList[blockListSize].col]->unlock();
		}
	}

}

void movePartitionN(SlidingPartition * localPartition){
	unsigned int negOne = -1;

	if(localPartition->blockList[0].row-1 > 1 && localPartition->blockList[0].row-1 != negOne){


			for(int i = localPartition->blockList.size()-1; i > 0; i--){
				if(grid[localPartition->blockList[0].row-1][localPartition->blockList[0].col] == SquareType::FREE_SQUARE){  
				
					if (localPartition->blockList[0].row-1 > 0){
						localPartition->blockList[i].row--;
						gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
						grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::VERTICAL_PARTITION;
						gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
						erasePartition(localPartition,Direction::NORTH);
					}
				}



			}




	}
}

void movePartitionS(SlidingPartition * localPartition){
int blockListSize = localPartition->blockList.size()-1;
	if(localPartition->blockList[blockListSize].row+1 < numRows-1  && localPartition->blockList[blockListSize].row+1 < numRows-1){
			for(int i = localPartition->blockList.size()-1; i > 0; i--){


				if(grid[localPartition->blockList[blockListSize].row+1][localPartition->blockList[blockListSize].col] == SquareType::FREE_SQUARE){ 					
					if(localPartition->blockList[blockListSize].row+1 <= numRows && localPartition->blockList[blockListSize].row+1 <= numRows){
							localPartition->blockList[i].row++;
							gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
							grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::VERTICAL_PARTITION;
							gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
							erasePartition(localPartition,Direction::SOUTH);
					}

				}
			
			}




		
	}

}
void movePartitionE(SlidingPartition * localPartition){
int blockListSize = localPartition->blockList.size()-1;

	

	if(localPartition->blockList[0].col+1 < numCols && localPartition->blockList[blockListSize].col+1 < numCols){
			for(int i = localPartition->blockList.size()-1; i > 0; i--){

				if(grid[localPartition->blockList[blockListSize].row][localPartition->blockList[blockListSize].col+1] == SquareType::FREE_SQUARE){
					if( localPartition->blockList[0].col+1 <= numCols  && localPartition->blockList[blockListSize].col+1  <= numCols ){
							localPartition->blockList[i].col++;
							gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
							grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::HORIZONTAL_PARTITION;
							gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
							erasePartition(localPartition,Direction::EAST);
					}
			
				}

			}




		
	}
}


void movePartitionW(SlidingPartition * localPartition){
	unsigned int negOne = -1;
int blockListSize = localPartition->blockList.size()-1;


	if(localPartition->blockList[0].col-1 > 0 && localPartition->blockList[0].col-1 != negOne){
			for(int i = localPartition->blockList.size()-1; i > 0; i--){
				if(grid[localPartition->blockList[0].row][localPartition->blockList[0].col-1] == SquareType::FREE_SQUARE){ 
					if(localPartition->blockList[0].col-1 >= 0&& localPartition->blockList[blockListSize].col-1 >= 0){
						localPartition->blockList[i].col--;
						gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
						grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::HORIZONTAL_PARTITION;
						gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
						erasePartition(localPartition,Direction::WEST);
					}
				}
			}
		
	}

}




void slidePartition(SlidingPartition * localPartition){
	/* Recieves partition &, determines if it is vertical, if so north, south become available, else east and west become availabe
	then a random direction gets selected, erase partition 
	*/
	
	vector<Direction> possibleDir;
		//cout<<localPartition->blockList[0].col--<<endl;
	if(localPartition->isVertical){
		possibleDir.push_back(Direction::NORTH);
		possibleDir.push_back(Direction::SOUTH);
	}
	
	else{
		possibleDir.push_back(Direction::EAST);
		possibleDir.push_back(Direction::WEST);
	}

	int pickDir = rand()%2;
	// calls following functs to update and redraw partitions in new positions
	//erasePartition(localPartition);

	if(possibleDir[pickDir] == Direction::NORTH){

		movePartitionN(localPartition);
	}
	if(possibleDir[pickDir] == Direction::SOUTH){
		movePartitionS(localPartition);
	}
	if(possibleDir[pickDir] == Direction::EAST){
		movePartitionE(localPartition);
	}
	if(possibleDir[pickDir] == Direction::WEST){
		movePartitionW(localPartition);
	}
}


void eraseTraveler(struct Traveler * localTraveler){
//Free up squares
	//mutexLock.lock();
	for(int i = localTraveler->segmentList.size()-1; i >= 0; i--){
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->lock();

		if(grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] != SquareType::EXIT){
			grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] = SquareType::FREE_SQUARE;
		}

		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->unlock();
	}

// deallocate Traveler passed

		
	localTraveler->segmentList.erase(localTraveler->segmentList.begin(),localTraveler->segmentList.begin()+ localTraveler->segmentList.size());
	//mutexLock.unlock();
}


void updatePos(struct Traveler * localTraveler){
	mutexLock.lock();
	for(int i = localTraveler->segmentList.size()-1; i >= 0; i--){
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->lock();
		grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] = SquareType::FREE_SQUARE;
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->unlock();
	}
	mutexLock.unlock();



	mutexLock.lock();
	localTraveler->travelerLock->lock();

	for(int i = localTraveler->segmentList.size()-1; i > 0; i--){
		localTraveler->segmentList[i].col=localTraveler->segmentList[i-1].col;
		localTraveler->segmentList[i].row=localTraveler->segmentList[i-1].row;
		localTraveler->segmentList[i].dir=localTraveler->segmentList[i-1].dir;
	}

	localTraveler->travelerLock->unlock();
	mutexLock.unlock();


}

void updateTravelerBlocks(struct Traveler * localTraveler){
		mutexLock.lock();
	for(int i = localTraveler->segmentList.size()-1; i >= 0; i--){
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->lock();
		
		if(grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] != SquareType::EXIT)
		{
			grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] = SquareType::TRAVELER;
		}
		
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->unlock();
	}
		mutexLock.unlock();

}

void growTraveler(struct Traveler * localTraveler){
		mutexLock.lock();
		int lastElement = localTraveler->segmentList.size()-1;
		TravelerSegment seg = {localTraveler->segmentList[lastElement].row, localTraveler->segmentList[lastElement].col, localTraveler->segmentList[lastElement].dir}; 
		localTraveler->travelerLock->lock();
		localTraveler->segmentList.push_back(seg);

		localTraveler->travelerLock->unlock();
		mutexLock.unlock();

		updateTravelerBlocks(localTraveler);
}



void moveTravelerN(struct Traveler * localTraveler){
 	updatePos(localTraveler);


	mutexLock.lock();
	localTraveler->travelerLock->lock();
	localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::NORTH;
	localTraveler->segmentList[0].row--;
	localTraveler->travelerLock->unlock();
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);
}


void moveTravelerS(struct Traveler * localTraveler){
 	updatePos(localTraveler);

	mutexLock.lock();
	localTraveler->travelerLock->lock();
	localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::SOUTH;
	localTraveler->segmentList[0].row++;
	localTraveler->travelerLock->unlock();
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);
}


void moveTravelerE(struct Traveler * localTraveler){

	updatePos(localTraveler);

	mutexLock.lock();
	localTraveler->travelerLock->lock();
	localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::EAST;
	localTraveler->segmentList[0].col++;
	localTraveler->travelerLock->unlock();
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);

}

void moveTravelerW(struct Traveler * localTraveler){
 	updatePos(localTraveler);

	mutexLock.lock();
	localTraveler->travelerLock->lock();
	localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::WEST;
	localTraveler->segmentList[0].col--;
	localTraveler->travelerLock->unlock();
	mutexLock.unlock();
  
	updateTravelerBlocks(localTraveler);

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
			appRunning = false;
			for(unsigned int i = 0; i < threadList.size(); i++){
				threadList[i]->join();
			}
			exit(0);
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
		for(unsigned int i = 0; i < partitionList.size()-1; i++){
		slidePartition(&partitionList[i]);

		}
			speedupTravelers();
			ok = 1;
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
	numCols = atoi(argv[1]);
	numRows = atoi(argv[2]);
	numTravelers = atoi(argv[3]);
	movesBeforeGrowth = atoi(argv[4]);
	//cout<<movesBeforeGrowth<<endl;
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
	for (unsigned int i=0; i< numRows; i++)
		free(gridLocks[i]);
	free(gridLocks);

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
	gridLocks = new mutex**[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		gridLocks[i] = new mutex*[numCols];
		for (unsigned int j=0; j< numCols; j++){
			grid[i][j] = SquareType::FREE_SQUARE;
			gridLocks[i][j] = new mutex;
		}

		
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

	gridLocks[exitPos.row][exitPos.col]->lock();
	grid[exitPos.row][exitPos.col] = SquareType::EXIT;
	gridLocks[exitPos.row][exitPos.col]->unlock();

	//	Generate walls and partitions
	generateWalls();
	generatePartitions();

	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float** travelerColor = createTravelerColors(numTravelers);
	for (unsigned int k=0; k<numTravelers; k++) {
		mutex travelerLock;
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.segmentList.push_back(seg);

		gridLocks[pos.row][pos.col]->lock();
		grid[pos.row][pos.col] = SquareType::TRAVELER;
		gridLocks[pos.row][pos.col]->unlock();

		//	I add 0-n segments to my travelers
		unsigned int numAddSegments = segmentNumberGenerator(engine);
		TravelerSegment currSeg = traveler.segmentList[0];
		bool canAddSegment = true;
		//cout << "Traveler " << k << " at (row=" << pos.row << ", col=" <<
		//pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
		//cout << "\t";

		for (unsigned int s=0; s<numAddSegments && canAddSegment; s++)
		{
			TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
			if (canAddSegment)
			{
				traveler.segmentList.push_back(newSeg);
				currSeg = newSeg;
			}
		}

		for (unsigned int c=0; c<4; c++)
			traveler.rgba[c] = travelerColor[k][c];

		travelerList.push_back(traveler);
	}

	//Loop to create threads and call the thread function passing a pointer to a traveler
	thread** travelerThreads = new thread*[numTravelers];
	for(unsigned int i = 0; i < numTravelers; i++){
		travelerThreads[i] = new thread(singleThreadFunc, &travelerList[i]);
		threadList.push_back(travelerThreads[i]);
		numLiveThreads++;
	}


	
	//	free array of colors
	for (unsigned int k=0; k<numTravelers; k++)
		delete []travelerColor[k];
	delete []travelerColor;
	
}


/*
	Dylan
	Function called by threads moves the traveler until they reach the goal or get stuck
*/
void singleThreadFunc(struct Traveler *localTraveler){

	//Initialize mutex lock for traveler
	localTraveler->travelerLock = new mutex;
	
	bool goalReached = false;
	unsigned int currentRow, currentCol;

	//Loop to move the traveler while the app is still running and the goal is not reached
	while(goalReached != true && appRunning == true){
		mutexLock.lock();
		currentRow = localTraveler->segmentList[0].row;
		currentCol = localTraveler->segmentList[0].col;
		mutexLock.unlock();

		//Check if goal is reached
		if(currentRow == exitPos.row && currentCol == exitPos.col){
			goalReached = true;
			numLiveThreads --;
			numTravelersDone ++;
			eraseTraveler(localTraveler);
		}
		//Moves the traveler if the goal has not been reached yet
		if(goalReached == false){
			moveTraveler(localTraveler);
		}

	}
			
}


/*
	Dylan
	Function called by thread function to figure out what direction to move the traveler
*/
void moveTraveler(struct Traveler *localTraveler){
	//Tracks all directions a traveler can move
	vector<Direction> canMove;
	vector<Direction> partitionCanMove;
	Direction behind;
	
	int moves = 0;
	int partitionMoves = 0;
	int currentRow = localTraveler->segmentList[0].row;
	int currentCol = localTraveler->segmentList[0].col;

	unsigned int negativeOne = -1;
	
	//Keeps track of the row/column displacement for a move in a certain direction
	unsigned int northAdjustment = localTraveler->segmentList[0].row - 1;
	unsigned int southAdjustment = localTraveler->segmentList[0].row + 1;
	unsigned int westAdjustment = localTraveler->segmentList[0].col - 1;
	unsigned int eastAdjustment = localTraveler->segmentList[0].col + 1;
	
	//Used to see if the square to the direction is open or the exit & in bounds
	bool northOpen = false;
	bool southOpen = false;
	bool westOpen = false;
	bool eastOpen = false;

	//Used to see if the square to the direction is a partition
	bool northPartition = false;
	bool southPartition = false;
	bool westPartition = false;
	bool eastPartition = false;

mutexLock.lock();
	//Find direction that is behind it
	if(localTraveler->segmentList[0].dir == Direction::NORTH){
		behind = Direction::SOUTH;
	}
	else if(localTraveler->segmentList[0].dir == Direction::SOUTH){
		behind = Direction::NORTH;
	}
	else if(localTraveler->segmentList[0].dir == Direction::EAST){
		behind = Direction::WEST;
	}
	else{
		behind = Direction::EAST;
	}
mutexLock.unlock();

	mutexLock.lock();
	//Checks if moving that direction is in bounds and an acceptable tile to move to
	if (northAdjustment >= 0 && northAdjustment != negativeOne){
		gridLocks[northAdjustment][currentCol]->lock();
		if(grid[northAdjustment][currentCol] == SquareType::FREE_SQUARE || grid[northAdjustment][currentCol] == SquareType::EXIT){
			northOpen = true;
		}

		else if(grid[northAdjustment][currentCol] == SquareType::VERTICAL_PARTITION){
			northPartition = true;
			partitionMoves++;
		}

		gridLocks[northAdjustment][currentCol]->unlock();
	}
	mutexLock.unlock();

	mutexLock.lock();
	if (southAdjustment < numRows){
		gridLocks[southAdjustment][currentCol]->lock();
		if(grid[southAdjustment][currentCol] == SquareType::FREE_SQUARE || grid[southAdjustment][currentCol] == SquareType::EXIT){
			southOpen = true;
		}
		else if(grid[southAdjustment][currentCol] == SquareType::VERTICAL_PARTITION){
			southPartition = true;
			partitionMoves++;
		}
		gridLocks[southAdjustment][currentCol]->unlock();
	}
	mutexLock.unlock();
	mutexLock.lock();
	if (westAdjustment >= 0 && westAdjustment != negativeOne){
		gridLocks[currentRow][westAdjustment]->lock();
		if(grid[currentRow][westAdjustment] == SquareType::FREE_SQUARE || grid[currentRow][westAdjustment] == SquareType::EXIT){
			westOpen = true;
		}
		else if(grid[currentRow][westAdjustment] == SquareType::HORIZONTAL_PARTITION){
			westPartition = true;
			partitionMoves++;
		}
		gridLocks[currentRow][westAdjustment]->unlock();
	}
	mutexLock.unlock();

	mutexLock.lock();
	if (eastAdjustment < numCols){
		gridLocks[currentRow][eastAdjustment]->lock();
		if(grid[currentRow][eastAdjustment] == SquareType::FREE_SQUARE || grid[currentRow][eastAdjustment] == SquareType::EXIT){
			eastOpen = true;
		}
		else if(grid[currentRow][eastAdjustment] == SquareType::HORIZONTAL_PARTITION){
			eastPartition = true;
			partitionMoves++;
		}
		gridLocks[currentRow][eastAdjustment]->unlock();
	}
	mutexLock.unlock();

	mutexLock.lock();
	//Puts all the possible moves in the vector
	if(Direction::NORTH != behind && northOpen == true){
		canMove.push_back(Direction::NORTH);
		moves++;
	}

	if(Direction::SOUTH != behind && southOpen == true){
		canMove.push_back(Direction::SOUTH);
		moves++;
	}

	if(Direction::WEST != behind && westOpen == true){
		canMove.push_back(Direction::WEST);
		moves++;
	}

	if(Direction::EAST != behind && eastOpen == true){
		canMove.push_back(Direction::EAST);
		moves++;
	}


	if(Direction::NORTH != behind && northPartition == true){
		partitionCanMove.push_back(Direction::NORTH);
		moves++;
	}

	if(Direction::SOUTH != behind && southPartition == true){
		partitionCanMove.push_back(Direction::SOUTH);
		moves++;
	}

	if(Direction::WEST != behind && westPartition == true){
		partitionCanMove.push_back(Direction::WEST);
		moves++;
	}

	if(Direction::EAST != behind && eastPartition == true){
		partitionCanMove.push_back(Direction::EAST);
		moves++;
	}
	mutexLock.unlock();

	//If it can move, it chooses a random direction and goes that way
	if(moves > 0){
		if(localTraveler->movesTraveled % movesBeforeGrowth == 0){
			growTraveler(localTraveler);
		/* 
		Grow traveler
		*/

		}
		moveDirection(localTraveler, canMove[rand() % moves]);
	}
	else if(partitionMoves > 0){
		if(localTraveler->movesTraveled % movesBeforeGrowth == 0){
			growTraveler(localTraveler);
		/* 
		Grow traveler
		*/
		}

		moveDirectionParition(localTraveler, partitionCanMove[rand() % moves]);
		
	}
}

/*
	Dylan
	Function that takes a traveler and a direction and calls the corresponding move function
*/
void moveDirection(struct Traveler *localTraveler, Direction currentDir){

	if(currentDir == Direction::NORTH){
		moveTravelerN(localTraveler);
		usleep(travelerSleepTime);
	}

	else if(currentDir == Direction::SOUTH){
		moveTravelerS(localTraveler);
		usleep(travelerSleepTime);
	}
	
	else if(currentDir == Direction::EAST){
		moveTravelerE(localTraveler);
		usleep(travelerSleepTime);
	}

	else if(currentDir == Direction::WEST){
		moveTravelerW(localTraveler);
		usleep(travelerSleepTime);
	}
	
}


/*
	Dylan
	Function that takes a traveler and a direction and calls the corresponding partition move function
*/
void moveDirectionParition(struct Traveler *localTraveler, Direction currentDir){
	
	int currentRow = localTraveler->segmentList[0].row;
	int currentCol = localTraveler->segmentList[0].col;
	int partitionIdx, blockIdx;
	
	if(currentDir == Direction::NORTH){
		unsigned int northAdjustment = localTraveler->segmentList[0].row - 1;
		for (unsigned int i = 0; i < partitionList.size(); i++){
			if(partitionList[i].blockList[0].row == northAdjustment || partitionList[i].blockList[0].col == currentCol){
				for(unsigned int j = 0; j < partitionList[i].blockList.size(); j++){
					if(partitionList[i].blockList[0].row == northAdjustment && partitionList[i].blockList[0].col == currentCol){
						partitionIdx = i;
						blockIdx = j;
					}
				}
			}
		}
		
		movePartitionN(&partitionList[partitionIdx]);
		usleep(travelerSleepTime);
	}

	else if(currentDir == Direction::SOUTH){
		unsigned int southAdjustment = localTraveler->segmentList[0].row + 1;
		for (unsigned int i = 0; i < partitionList.size(); i++){
			if(partitionList[i].blockList[0].row == southAdjustment || partitionList[i].blockList[0].col == currentCol){
				for(unsigned int j = 0; j < partitionList[i].blockList.size(); j++){
					if(partitionList[i].blockList[0].row == southAdjustment && partitionList[i].blockList[0].col == currentCol){
						partitionIdx = i;
						blockIdx = j;
					}
				}
			}
		}
		
		movePartitionS(&partitionList[partitionIdx]);
		usleep(travelerSleepTime);
	}
	
	else if(currentDir == Direction::WEST){
	unsigned int westAdjustment = localTraveler->segmentList[0].col - 1;
		for (unsigned int i = 0; i < partitionList.size(); i++){
			if(partitionList[i].blockList[0].row == currentRow || partitionList[i].blockList[0].col == westAdjustment){
				for(unsigned int j = 0; j < partitionList[i].blockList.size(); j++){
					if(partitionList[i].blockList[0].row == currentRow && partitionList[i].blockList[0].col == westAdjustment){
						partitionIdx = i;
						blockIdx = j;
					}
				}
			}
		}

		movePartitionW(&partitionList[partitionIdx]);
		usleep(travelerSleepTime);
	}

	else if(currentDir == Direction::EAST){
		unsigned int eastAdjustment = localTraveler->segmentList[0].col + 1;
		for (unsigned int i = 0; i < partitionList.size(); i++){
			if(partitionList[i].blockList[0].row == currentRow || partitionList[i].blockList[0].col == eastAdjustment){
				for(unsigned int j = 0; j < partitionList[i].blockList.size(); j++){
					if(partitionList[i].blockList[0].row == currentRow && partitionList[i].blockList[0].col == eastAdjustment){
						partitionIdx = i;
						blockIdx = j;
					}
				}
			}
		}
		
		movePartitionE(&partitionList[partitionIdx]);
		usleep(travelerSleepTime);
	}
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
		gridLocks[row][col]->try_lock();
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
		gridLocks[row][col]->unlock();
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
					gridLocks[row][col]->lock();
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
					gridLocks[row][col]->unlock();
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						gridLocks[row][col]->lock();
						grid[row][col] = SquareType::WALL;
						gridLocks[row][col]->unlock();
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
					gridLocks[row][col]->lock();
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
					gridLocks[row][col]->unlock();
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						gridLocks[row][col]->lock();
						grid[row][col] = SquareType::WALL;
						gridLocks[row][col]->unlock();
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
					gridLocks[row][col]->lock();
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
					gridLocks[row][col]->unlock();
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						gridLocks[row][col]->lock();
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						gridLocks[row][col]->unlock();
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
				partitionList.push_back(part);	
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
					gridLocks[row][col]->lock();
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
					gridLocks[row][col]->unlock();	
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						gridLocks[row][col]->lock();
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						gridLocks[row][col]->unlock();
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
			
		}
	}
}