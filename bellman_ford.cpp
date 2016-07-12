#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <limits.h>

using namespace std;

/* Count the number of vertex in the given graph. */
int countSize(string str)
{
	int count = 0;
	for(int i = 0; i < str.size(); i++)
	{
		if(str[i] == ',') count++;
	}
	count++;
	return count;
}

/* Transform a comma-seperate-form string to corresponding integer array */
void splitStringtoInt(string str, int* edges)
{

	int index = 0;
	int num = -1;
	stringstream ss(str);
	string tempstr;

	while(getline(ss, tempstr, ','))
	{
		// If it is *, it's weight is -1, meaning no egde.
		if(tempstr[0] =='*')
		{
			edges[index] = -1;
			index++;
		}
		else
		{
			edges[index] = atoi(tempstr.c_str());
			index++;
		}
	}
}

// Ouput the Bellman Ford result to a output file
void outputResult(int vnum, int* distance, int* predecessor, int iteration, string filename)
{
	string ofilebname = filename.substr(0, filename.size()-4) + "_output.txt";
	ofstream ofile(ofilebname.c_str());
	if(!ofile.is_open())
	{
		cerr << "Fail to create output file" << endl;
		return;
	}

	ofile << "0";
	for(int i = 1; i < vnum; i++)
	{
		ofile << "," << distance[i];
	}
	ofile << endl;

	ofile << "0" << endl;
	vector<int> sequence;
	for(int i = 1; i < vnum; i++)
	{
		//ofile << i << "<-" << predecessor[i] << endl;
		sequence.clear();
		int pred = predecessor[i];
		while(pred != -1)
		{
			sequence.push_back(pred);
			pred = predecessor[pred];
		}
		while(!sequence.empty())
		{
			ofile << sequence.back() << "->";
			sequence.pop_back();
		}
		ofile << i << endl;
	}
	ofile << "Iteration:" << iteration;
}

// The implementation of Bellman Ford Algorithm.
void bellmanFord(int **mgraph, int v, string outputfile)
{
	int d[v];
	int pred[v];
	bool identical = true;
	int iter = 0;

	// Initialization.
	for(int i = 0; i < v; i++)
	{
		d[i] = INT_MAX;
		pred[i] = -1;
	}
	d[0] = 0;
	vector<int> updatedNodes;

	// Step 1: dynamics programming at most v-1 iterations.
	for(int i = 1; i < v; i++)
	{
		iter++;
		updatedNodes.clear();
		// Step 2: Find all nodes that been updated from the previous iterations
		for(int h = 0; h < v; h++)
		{
			if(d[h] != INT_MAX)
			{
				updatedNodes.push_back(h);
			}
		}

		// Update d[x] for x belongs to updateNodes.
		for (unsigned q = 0; q < updatedNodes.size(); q++) 
		{
			int s = updatedNodes[q];
			for(int k = 0; k < v; k++)
			{
				if(mgraph[s][k] == 0 || mgraph[s][k] == -1) continue;
				if(d[k] > d[s] + mgraph[s][k])
				{
					d[k] = d[s] + mgraph[s][k];
					pred[k] = s;
					identical = false;
				}
			}
		}
		if(identical) break;
		else identical = true;
	}
	// Output the result.
	outputResult(v, d, pred, iter, outputfile);
}

int main(int argc, char** argv)
{
	if(argc != 2) 
	{
		cerr << "Argument Error" << endl;
		return 0;
	}

	ifstream infile;
	infile.open(argv[1]);
	if(!infile.is_open())
	{
		cerr << "Fail to open the file" << endl;
		return 0;
	}

	// Initilize the graph
	int **graph;
	int num = 0;
	int v = 0;
	string line;

	// Get the first line in the .csv file.
	if(getline(infile, line))
	{
		// Count the number of vertex.
		v = countSize(line);
		graph = new int*[v];
		for(int i = 0; i < v; i++)
		{
			graph[i] = new int[v];
		}
		// Initialize the graph with the first line of data.
		splitStringtoInt(line, graph[num]);
		num++;
	}


	// Initialize the graph with the rest of lines of data.
	while(getline(infile, line))
	{
		splitStringtoInt(line, graph[num]);
		num++;
	}

	// Run Bellman Ford Algorithm.
	bellmanFord(graph, v, string(argv[1]));

	// Release the memeory allocated.
	for(int i = 0; i < v; i++)
	{
		delete [] graph[i];
	}

	delete [] graph;
}

