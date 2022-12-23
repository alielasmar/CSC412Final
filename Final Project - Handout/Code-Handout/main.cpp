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
Direction findMoveDirection(struct Traveler *localTraveler);
void moveTraveler(struct Traveler *localTraveler);
void moveDirection(struct Traveler *localTraveler, Direction currentDir);

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
bool appRunning = true;

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

void erasePartition(SlidingPartition * localPartition){
	for(int i = localPartition->blockList.size()-1; i >= 0; i--){

		gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
		grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::FREE_SQUARE;
		gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
	}
}

void movePartition(SlidingPartition * localPartition, Direction directionMoving){

	if(directionMoving == Direction::NORTH){
		for(int i = localPartition->blockList.size()-1; i > 0; i--){
			localPartition->blockList[i].row--;

			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
			grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::VERTICAL_PARTITION;
			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
		}
	}


	if(directionMoving == Direction::SOUTH){
		for(int i = localPartition->blockList.size()-1; i > 0; i--){
			localPartition->blockList[i].row++;

			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
			grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::VERTICAL_PARTITION;
			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
		}
	}


	if(directionMoving == Direction::EAST){

		for(int i = localPartition->blockList.size()-1; i > 0; i--){
			localPartition->blockList[i].col++;

			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
			grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::HORIZONTAL_PARTITION;
			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
		}
	}


	if(directionMoving == Direction::WEST){
		for(int i = localPartition->blockList.size()-1; i > 0; i--){
			localPartition->blockList[i].col++;

			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->lock();
			grid[localPartition->blockList[i].row][localPartition->blockList[i].col] = SquareType::HORIZONTAL_PARTITION;
			gridLocks[localPartition->blockList[i].row][localPartition->blockList[i].col]->unlock();
		}		
	}
}

void slidePartition(SlidingPartition * localPartition){
	//cout<<"SLIDE"<<endl;
	
	vector<Direction> possibleDir;
		//cout<<localPartition->blockList[0].col--<<endl;
	if(localPartition->isVertical){
		cout<<"VERTICAL"<<endl;

		possibleDir.push_back(Direction::NORTH);
		possibleDir.push_back(Direction::SOUTH);
	}
	
	else{
		possibleDir.push_back(Direction::EAST);
		possibleDir.push_back(Direction::WEST);
	}

	int pickDir = rand()%2;

		erasePartition(localPartition);
	movePartition(localPartition,possibleDir[pickDir]);

}


void eraseTraveler(struct Traveler * localTraveler){
//Free up squares
	mutexLock.lock();
	for(int i = localTraveler->segmentList.size()-1; i >= 0; i--){
		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->lock();

		if(grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] != SquareType::EXIT){
			grid[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col] = SquareType::FREE_SQUARE;
		}

		gridLocks[localTraveler->segmentList[i].row][localTraveler->segmentList[i].col]->unlock();
	}

// deallocate Traveler passed

		
	localTraveler->segmentList.erase(localTraveler->segmentList.begin(),localTraveler->segmentList.begin()+ localTraveler->segmentList.size());
	mutexLock.unlock();
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
	for(int i = localTraveler->segmentList.size()-1; i > 0; i--){
		localTraveler->segmentList[i].col=localTraveler->segmentList[i-1].col;
		localTraveler->segmentList[i].row=localTraveler->segmentList[i-1].row;
		localTraveler->segmentList[i].dir=localTraveler->segmentList[i-1].dir;

	}
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
		localTraveler->segmentList.push_back(seg);
		mutexLock.unlock();

		updateTravelerBlocks(localTraveler);
}



void moveTravelerN(struct Traveler * localTraveler){
 	updatePos(localTraveler);

	mutexLock.lock();
			localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::NORTH;
	localTraveler->segmentList[0].row--;
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);
}


void moveTravelerS(struct Traveler * localTraveler){
 	updatePos(localTraveler);

	mutexLock.lock();
			localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::SOUTH;
	localTraveler->segmentList[0].row++;
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);
}


void moveTravelerE(struct Traveler * localTraveler){

	updatePos(localTraveler);
	mutexLock.lock();
			localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::EAST;
	localTraveler->segmentList[0].col++;
	mutexLock.unlock();
	updateTravelerBlocks(localTraveler);

}

void moveTravelerW(struct Traveler * localTraveler){
 	updatePos(localTraveler);

	mutexLock.lock();
			localTraveler->movesTraveled++;
	localTraveler->segmentList[0].dir = Direction::WEST;
	localTraveler->segmentList[0].col--;
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
				//cout << dirStr(newSeg.dir) << "  ";
			}
		}
		//cout << endl;

		for (unsigned int c=0; c<4; c++)
			traveler.rgba[c] = travelerColor[k][c];

		travelerList.push_back(traveler);
	}

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


void singleThreadFunc(struct Traveler *localTraveler){

	localTraveler->travelerLock = new mutex;

	bool goalReached = false;
	unsigned int currentRow, currentCol;

	while(goalReached != true && appRunning == true){
		mutexLock.lock();
		currentRow = localTraveler->segmentList[0].row;
		currentCol = localTraveler->segmentList[0].col;
		mutexLock.unlock();

		if(currentRow == exitPos.row && currentCol == exitPos.col){
			goalReached = true;
			numLiveThreads --;
			numTravelersDone ++;
			eraseTraveler(localTraveler);
			
			//jyh
			//	You will also want to clear the grid squares that are still marked
			//	as occupied by your traveler.
		}
		//mutexLock.unlock();
		if(goalReached == false){

			moveTraveler(localTraveler);
		}

	}
			
}



void moveTraveler(struct Traveler *localTraveler){
	vector<Direction> canMove;
	Direction behind;
	int moves = 0;
	int currentRow = localTraveler->segmentList[0].row;
	int currentCol = localTraveler->segmentList[0].col;
	unsigned int negativeOne = -1;
	unsigned int northAdjustment = localTraveler->segmentList[0].row - 1;
	unsigned int southAdjustment = localTraveler->segmentList[0].row + 1;
	unsigned int westAdjustment = localTraveler->segmentList[0].col - 1;
	unsigned int eastAdjustment = localTraveler->segmentList[0].col + 1;
	bool northOpen = false;
	bool southOpen = false;
	bool westOpen = false;
	bool eastOpen = false;

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
	if (northAdjustment >= 0 && northAdjustment != negativeOne){
		gridLocks[northAdjustment][currentCol]->lock();
		if(grid[northAdjustment][currentCol] == SquareType::FREE_SQUARE || grid[northAdjustment][currentCol] == SquareType::EXIT){
			northOpen = true;
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
		gridLocks[southAdjustment][currentCol]->unlock();
	}
	mutexLock.unlock();
	mutexLock.lock();
	if (westAdjustment >= 0 && westAdjustment != negativeOne){
		gridLocks[currentRow][westAdjustment]->lock();
		if(grid[currentRow][westAdjustment] == SquareType::FREE_SQUARE || grid[currentRow][westAdjustment] == SquareType::EXIT){
			westOpen = true;
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
		gridLocks[currentRow][eastAdjustment]->unlock();
	}
	mutexLock.unlock();

	mutexLock.lock();
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
	mutexLock.unlock();


	if(moves > 0){
			if(localTraveler->movesTraveled % movesBeforeGrowth == 0){
				growTraveler(localTraveler);
			/* 
			Grow traveler
			*/

			}
		moveDirection(localTraveler, canMove[rand() % moves]);
	}
}

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
Direction findMoveDirection(struct Traveler *localTraveler){
	vector<Direction> canMove;
	Direction behind;
	int possibleDir = 0;
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
	
	//Find directions it can travel
	for(unsigned int i = 0; i < possibleDirections.size(); i++){
		if(possibleDirections[i] != behind && checkNextSquare(localTraveler, possibleDirections[i]) == true){
			canMove.push_back(possibleDirections[i]);
			possibleDir ++;
		}
	}
	//Pick a direction and and travel or say it can't move
	if(possibleDir != 0){
		return canMove[0];
	}
	else{
		return;
	}
}
*/


bool checkNextSquare(struct Traveler *localTraveler, Direction currentDir){
	mutexLock.lock();
	unsigned int currentRow = localTraveler->segmentList[0].row;
	unsigned int currentCol = localTraveler->segmentList[0].col;

	if (currentDir == Direction::EAST){
		gridLocks[currentCol + 1][currentRow]->lock();
		if(grid[currentCol + 1][currentRow] == SquareType::FREE_SQUARE && currentCol + 1 < numCols){
			return true;
		}
		else{
			return false;
		}
		gridLocks[currentCol + 1][currentRow]->unlock();
	}
	else if(currentDir == Direction::WEST){
		gridLocks[currentCol - 1][currentRow]->lock();
		if(grid[currentCol - 1][currentRow] == SquareType::FREE_SQUARE && currentCol - 1 > 0){
			return true;
		}
		else{
			return false;
		}
		gridLocks[currentCol - 1][currentRow]->unlock();
	}
	else if(currentDir == Direction::NORTH){
		gridLocks[currentCol][currentRow - 1]->lock();
		if(grid[currentCol][currentRow - 1] == SquareType::FREE_SQUARE && currentRow - 1 > 0){
			return true;
		}
		else{
			return false;
		}
		gridLocks[currentCol][currentRow - 1]->unlock();
	}
	else{
		gridLocks[currentCol][currentRow + 1]->lock();
		if(grid[currentCol][currentRow + 1] == SquareType::FREE_SQUARE && currentRow + 1 < 0){
			return true;
		}
		else{
			return false;
		}
		gridLocks[currentCol][currentRow + 1]->unlock();
	}
	mutexLock.unlock();
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