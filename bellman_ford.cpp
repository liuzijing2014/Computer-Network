#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>

using namespace std;

int countSize(string str)
{
	int count = 0;
	for(int i = 0; i < str.size(); i++)
	{
		if(str[i] == ',') continue;
		count++;
	}
	return count;
}

void splitStringtoInt(string str, int* edges)
{

	int index = 0;
	for(int i = 0; i < str.size(); i++)
	{
		if(str[i] == '*')
		{
			edges[index] = -1;
			index++;
		}
		else if(str[i] == ',') 
		{
			continue;
		}
		else
		{
			int k = atoi(&str[i]);
			edges[index] = k;
			index++;
		}
	}
}

int main(int argc, char** argv)
{
	if(argc != 2) 
	{
		cerr << "Argument Error";
		return 0;
	}

	int **graph;
	int num = 0;
	ifstream infile;
	infile.open(argv[1]);

	// Read in graph data and initilize the graph
	string line;
	getline(infile, line);
	int v = countSize(line);
	graph = new int*[v];
	for(int i = 0; i < v; i++)
	{
		graph[i] = new int[v];
	}
	splitStringtoInt(line, graph[num]);
	num++;

	while(getline(infile, line))
	{
		splitStringtoInt(line, graph[num]);
		num++;
	}

	for(int i = 0; i < v; i++)
	{
		for(int j = 0; j < v; j++)
		{
			cout << graph[i][j] <<',';
		}
		cout << endl;
	}

	for(int i = 0; i < v; i++)
	{
		delete [] graph[i];
	}

	delete [] graph;

}

