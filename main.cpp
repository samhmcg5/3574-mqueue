#include "external.h"
#include <unistd.h>

QTextStream qCout(stdout);
QTextStream qErr(stderr);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

   /*#############################
   # Handle Arguments and Errors #
   #############################*/
    // Check for command line errors
    if (checkErrors(argc, argv) == -1) {
        return EXIT_FAILURE;
    }

    // get the starting temperature
    std::istringstream ss(argv[2]);
    float temp;
    ss >> temp;

    // Get the name of this node
    int nodeNum;
    char const * self = getName(nodeNum, argv);

    // determine names of parents and children
    QList<char const *> children = getChildren(nodeNum);
    char const * parent = getParent(nodeNum);


    /*#############################
    #     Open Message Queues     #
    #############################*/


    // self
    mqSelf = mq_open(self, O_RDONLY | O_CREAT | O_EXCL, 0666, 0);
    if (mqSelf == -1)
    {
        qCout << "Error on open: " <<  errno << endl;
        closeAll(nodeNum);
        mq_unlink(self);
        return -1;
    }

    // children
    if (children.length())
    {
        mqChildL = mq_open(children[0], O_RDWR, 0, 0);
        if (mqChildL == -1)
        {
            qCout << "Error on open: " << errno << endl;
            closeAll(nodeNum);
            mq_unlink(self);
            return -1;
        }

        mqChildR = mq_open(children[1], O_RDWR, 0, 0);
        if (mqChildL == -1)
        {
            qCout << "Error on open: " << errno << endl;
            closeAll(nodeNum);
            mq_unlink(self);
            return -1;
        }
    }


    // Wait for All Connections to establish properly
    //sleep(2);




    /*#############################
    #     Main Functionality      #
    #############################*/
    // If I am node 0, send the first message, else enter loop
    float downTemp, upL, upR, upTemp;
    memset(buffer, bufsize, bufsize);
    sprintf(buffer,"%4f",temp);
    // main receive, then calculate, then send logic
    bool ENDED = false;
    bool PARENT_OPEN = false;
    char strbuf[1024];

    switch(nodeNum)
    {
    case 0:
        sprintf(strbuf,"Process #%2d: current temperature %3f",nodeNum,temp);
        qCout << strbuf << endl;
        while (!ENDED)
        {
            // To both children
            if (mq_send(mqChildL,buffer,strlen(buffer),0) == -1)
            {
                qErr << "Error on send (to childL): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            // send childR
            if (mq_send(mqChildR,buffer,strlen(buffer),0) == -1)
            {
                qErr << "Error on send (to childR): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }

            memset(buffer, bufsize, bufsize);

            // receive 1
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            upL = toFloat(buffer);
            memset(buffer, bufsize, bufsize);

            // receive 2
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            upR = toFloat(buffer);
            memset(buffer, bufsize, bufsize);
            upTemp = (temp + upR + upL)/3.0;
            sprintf(buffer,"%4f",upTemp);


            // Check final condition
            if ((temp-upTemp)>-0.01 && (temp-upTemp)<0.001)
            {
                ENDED = true;
                temp = upTemp;
                sprintf(strbuf,"Process #%2d: final temperature %3f",nodeNum,upTemp);
                qCout << strbuf << endl;

                sprintf(buffer,"FINAL");

                if (mq_send(mqChildL,buffer,bufsize,0) == -1)
                {
                    qErr << "Error on send (to childL): " << errno << endl;
                    closeAll(nodeNum);
                    mq_unlink(self);
                    return 1;
                }
                // send childR
                if (mq_send(mqChildR,buffer,bufsize,0) == -1)
                {
                    qErr << "Error on send (to childR): " << errno << endl;
                    closeAll(nodeNum);
                    mq_unlink(self);
                    return 1;
                }
                sleep(1);
                closeAll(nodeNum);
                return 0;
            }

            temp = upTemp;
            sprintf(strbuf,"Process #%2d: current temperature %3f",nodeNum,upTemp);
            qCout << strbuf << endl;


            // return to top


        }
        break;

    case 1:
    case 2:
        while (!ENDED)
        {
            //receive from parent
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }


            // FINAL CONDITION !
            if (QString(buffer) == "FINAL")
            {
                ENDED = true;
                sprintf(strbuf,"Process #%2d: final temperature %3f",nodeNum,temp);
                qCout << strbuf << endl;
                sprintf(buffer,"FINAL");

                if (mq_send(mqChildL,buffer,bufsize,0) == -1)
                {
                    qErr << "Error on send (to childL): " << errno << endl;
                    closeAll(nodeNum);
                    mq_unlink(self);
                    return 1;
                }
                // send childR
                if (mq_send(mqChildR,buffer,bufsize,0) == -1)
                {
                    qErr << "Error on send (to childR): " << errno << endl;
                    closeAll(nodeNum);
                    mq_unlink(self);
                    return 1;
                }

                sleep(1);
                closeAll(nodeNum);
                return 0;
            }

            downTemp = toFloat(buffer);
            temp = (downTemp + temp)/2.0;
            memset(buffer, bufsize, bufsize);
            sprintf(buffer,"%4f",temp);

            // open the parent !!
            if (!PARENT_OPEN)
            {
                mqParent = mq_open(parent, O_RDWR , 0666, NULL);
                if (mqParent == -1)
                    {
                        qCout << "Error on open: " << errno << endl;
                        closeAll(nodeNum);
                        mq_unlink(self);
                        return -1;
                    }
                PARENT_OPEN = true;
            }

            // send to both children
            if (mq_send(mqChildL,buffer,strlen(buffer),0) == -1)
            {
                qErr << "Error on send (to childL): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            // send childR
            if (mq_send(mqChildR,buffer,strlen(buffer),0) == -1)
            {
                qErr << "Error on send (to childR): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }

            memset(buffer, bufsize, bufsize);
            // receive from both children
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            upL = toFloat(buffer);
            memset(buffer, bufsize, bufsize);
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            upR = toFloat(buffer);
            memset(buffer, bufsize, bufsize);
            temp = (temp + upR + upL)/3.0;
            sprintf(buffer,"%4f",temp);

            sprintf(strbuf,"Process #%1d: current temperature %3f",nodeNum,temp);
            qCout << strbuf << endl;

            // send to parent
            if (mq_send(mqParent,buffer,bufsize,0) == -1)
            {
                qErr << "Error on send (to parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            // return to top
        }
        break;

    case 3:
    case 4:
    case 5:
    case 6:
        while (!ENDED)
        {
            //receive from parent
            if (mq_receive(mqSelf, buffer, bufsize, NULL) == -1)
            {
                qErr << "Error on receive (from parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }

            // FINAL CONDITION !
            if (QString(buffer) == "FINAL")
            {
                ENDED = true;
                sprintf(strbuf,"Process #%2d: final temperature %3f",nodeNum,temp);
                qCout << strbuf << endl;

                sleep(1);
                closeAll(nodeNum);
                return 0;
            }

            downTemp = toFloat(buffer);
            temp = (downTemp + temp)/2.0;
            memset(buffer, bufsize, bufsize);
            sprintf(buffer,"%4f",temp);


            // open the parent !!
            if (!PARENT_OPEN)
            {
                mqParent = mq_open(parent, O_RDWR , 0666, NULL);
                if (mqParent == -1)
                    {
                        qCout << "Error on open: " << errno << endl;
                        closeAll(nodeNum);
                        mq_unlink(self);
                        return -1;
                    }
                PARENT_OPEN = true;
            }

            sprintf(strbuf,"Process #%2d: current temperature %3f",nodeNum,temp);
            qCout << strbuf << endl;

            // send to parent
            if (mq_send(mqParent,buffer,strlen(buffer),0) == -1)
            {
                qErr << "Error on send (Parent): " << errno << endl;
                closeAll(nodeNum);
                mq_unlink(self);
                return 1;
            }
            // return to top
        }
        break;
    default:
        break;
    }


    closeAll(nodeNum);
    return 0;
}


void closeAll(int nodeNum)
{
    if (nodeNum)
        mq_close(mqParent);
    if (getChildren(nodeNum).length())
    {
        mq_close(mqChildL);
        mq_close(mqChildR);
    }
    mq_close(mqSelf);
    for (int i =0; i<NodeNames.length();++i)
    {
        mq_unlink(NodeNames[i]);
    }
}

float toFloat(char buf[])
{
    std::istringstream ss(buf);
    float ret;
    ss >>ret;
    return ret;
}


QList<char const *> getChildren(int nodeNum)
{
    QList<char const *> children;
    switch(nodeNum)
    {
    case 0:
        children.append(NodeNames[1]);
        children.append(NodeNames[2]);
        break;
    case 1:
        children.append(NodeNames[3]);
        children.append(NodeNames[4]);
        break;
    case 2:
        children.append(NodeNames[5]);
        children.append(NodeNames[6]);
        break;
    default:
        break;  // No children (nodes 3,4,5,6)
    }

    return children;
}

char const * getParent(int nodeNum)
{
    switch (nodeNum)
    {
    case 0:
        return NULL;
    case 1:
        return NodeNames[0];
    case 2:
        return NodeNames[0];
    case 3:
        return NodeNames[1];
    case 4:
        return NodeNames[1];
    case 5:
        return NodeNames[2];
    case 6:
        return NodeNames[2];
    default:
        return NULL;
        break;
    }
}

char const * getName(int & nodeNum, char * argv[])
{
    std::istringstream ss(argv[1]);
    ss >> nodeNum;

    switch (nodeNum) {
    case 0:
        return NodeNames[0];
    case 1:
        return NodeNames[1];
    case 2:
        return NodeNames[2];
    case 3:
        return NodeNames[3];
    case 4:
        return NodeNames[4];
    case 5:
        return NodeNames[5];
    case 6:
        return NodeNames[6];
    default:
        return NULL;
    }
}


int checkErrors(int argc, char * argv[])
{
    if (argc != 3) {
        qErr << "Usage: ./external <id> <initial-temp>" << endl;
        return -1;
    }

    int x;
    std::istringstream ss(argv[1]);
    if (!(ss >> x)) {
        qErr << "Type Error on node argument!" << endl;
        qErr << "Usage: ./external <int> <float>" << endl;
        return -1;
    }

    if (x > 6 || x < 0) {
        qErr << "Error! Node value out of range 0-6" << endl;
        return -1;
    }

    float y;
    std::istringstream ss2(argv[2]);
    if (!(ss2 >> y)) {
        qErr << "Type Error on temperature argument!" << endl;
        qErr << "Usage: ./external <int> <float>" << endl;
        return -1;
    }

    return 0;
}
