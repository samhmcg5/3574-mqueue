#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <QList>
#include <QCoreApplication>
#include <QTextStream>

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sstream>


/* Globals */
QList<char const *> NodeNames = {"/70", "/71", "/72", "/73", "/74", "/75", "/76"};
const std::size_t bufsize = 8192;
char buffer[bufsize];

mqd_t mqSelf = -1;
mqd_t mqChildR = -1;
mqd_t mqChildL = -1;
mqd_t mqParent = -1;


// returns -1 on failure, 0 on success
int checkErrors(int argc, char * argv[]);

// get the name of the queue based on the argument
char const * getName(int & nodeNum, char * argv[]);

// return QList of names of children
QList<char const *> getChildren(int nodeNum);

// return name of the parent
char const * getParent(int nodeNum);

//close all message queues if available
void closeAll(int nodeNum);

//convert the buffer to a float
float toFloat(char buf[]);


#endif // EXTERNAL_H
