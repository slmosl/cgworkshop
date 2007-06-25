#include "GraphHandler.h"

Graph GraphHandler::initial_graph = Graph();

GraphHandler::GraphHandler()  {

	//should copy the initial graph using copy c'tor
	m_igraph = new Graph();
}

GraphHandler::~GraphHandler() {
	delete m_igraph;
}

double counter =0;
int n=0;
double calcDist(CvMat * smoothness, int i, int j) {

	//static double sum = 0;
	//static int count = 0;
	double result=0;

	//double beta = 0.005837*10; // normalized
	double beta = 1782.446183 * 10; // unnormalized

	for (int k = 0; k<3; k++)
	{
		//printf("smooth %d: %f,%f\n", k, cvmGet(smoothness, i, k), cvmGet(smoothness, j, k));
		result += pow(cvmGet(smoothness, i, k) - cvmGet(smoothness, j, k),2);
	}
	//sum += result;
	//count ++;
	counter +=exp(-result/beta);
	n++;
	return exp(-result/beta);
}

void GraphHandler::init_graph(int rows, int cols, CvMat * smoothness) {

	//map <int, map<int, Graph::node_id>> node;
	
	//create m_nodes
	for (int i = 0; i <rows; i++) 
		for (int j = 0; j < cols; j++)
			m_nodes[i][j] = m_igraph->add_node();

	//need to somehow calculate beta here
 	double beta = 0.005837;

	for(int i = 1; i < rows-1; i += 2)
	{
		for (int j = 1; j < cols-1; j += 2) 
		{
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i-1][j-1], calcDist(smoothness, i*cols+j, (i-1)*cols+(j-1)));
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i][j-1], calcDist(smoothness, i*cols+j, i*cols+(j-1)));

			m_igraph->add_edge(m_nodes[i][j], m_nodes[i-1][j], calcDist(smoothness, i*cols+j, (i-1)*cols+j));
			
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i-1][j+1], calcDist(smoothness, i*cols +j, (i-1)*cols +(j+1)));		
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i][j+1], calcDist(smoothness, i*cols + j, i*cols + (j+1)));						
			
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i+1][j-1], calcDist(smoothness, i*cols+j, (i+1)*cols+(j-1)));						
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i+1][j], calcDist(smoothness, i*cols+j, (i+1)*cols+j));	
			
			m_igraph->add_edge(m_nodes[i][j], m_nodes[i+1][j+1], calcDist(smoothness, i*cols+j, (i+1)*cols+(j+1)));				
		}
	}

	printf("Smoothness avg %lf\n", counter/n);

}



void GraphHandler::assign_weights(CvMat *Bu, CvMat *Fu) {

	for (int i=0; i<Bu->rows; i++)
		for (int j=0; j<Bu->cols; j++) {
			//add the Sink E1 term
			m_igraph->set_tweights(m_nodes[i][j], 
										cvmGet(Fu,i,j),
										cvmGet(Bu,i,j));
														
	}


}

void GraphHandler::do_MinCut(CvMat &result) {

	m_flow = m_igraph->maxflow();
	int counter1 = 0;
	int counter2 = 0;


	for(int i=0; i<result.rows; i++)
		for (int j=0; j<result.cols; j++)
			//we can push it all into one line...
			if (m_igraph->what_segment(m_nodes[i][j])==Graph::SINK){
				counter1++;
				cvmSet(&result, i,j,0);
			}
			else {
				cvmSet(&result, i,j,1);
				counter2++;
				//printf("%d %d \n", i, j);
			}
			
	printf("COUNTER1 %d COUNTER2 %d", counter1, counter2);
}

