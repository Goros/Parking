#include "geometry.h"
#include "stdlib.h"
#include "stl_reader.h"
#include "dxf_reader.h"
#include "chunck3ds_reader.h"
#include "iges_reader.h"
#include <qdebug.h>

#ifdef WIN32
#include <Windows.h>
#include <WinGDI.h>
#include <gl/GL.h>
#else
#include <GL/glu.h>
#endif

using namespace std;

#include <set>
#include <ctime>
#include <cmath>

Geometry::Geometry()
{
	hasSmoothNormals=0;
	 
	pickedGrid=-1;

	edgeStrip=NULL;
	lineStrip=NULL;
	triaStripVertex=NULL;
	triaStripElement=NULL;

	edgeStripColor[0]=0;
	edgeStripColor[1]=0;
	edgeStripColor[2]=0;
	edgeStripColor[3]=0;

	lineStripColor[0]=0;
	lineStripColor[1]=0;
	lineStripColor[2]=0;
	lineStripColor[3]=0;


}


Geometry::~Geometry()
{
	free(edgeStrip);
	free(lineStrip);
}



int Geometry::addGrid(float x,float y,float z)
{
	Grid G;
	G.coords[0]=x;
	G.coords[1]=y;
	G.coords[2]=z;
	G.pos=grids.length();
	grids.append(G);
	
	
	if (grids.length()==1) {
		minn[0]=x; minn[1]=y; minn[2]=z;
		maxx[0]=x; maxx[1]=y; maxx[2]=z;
	} else {
		if (minn[0]>x) minn[0]=x;
		if (minn[1]>y) minn[1]=y;
		if (minn[2]>z) minn[2]=z;
		if (maxx[0]<x) maxx[0]=x;
		if (maxx[1]<y) maxx[1]=y;
		if (maxx[2]<z) maxx[2]=z;
	}
	return grids.length()-1;

}


int Geometry::addPoint(int n)
{
	points.append(n);
	return points.length()-1;
}

int Geometry::addLine(int n1,int n2)
{
	Line L;
	L.node[0]=n1;
	L.node[1]=n2;
	lines.append(L);
	return lines.length()-1;
}

int Geometry::addEdge(int n1,int n2)
{
	Line L;
	L.node[0]=n1;
	L.node[1]=n2;
	edges.append(L);
	return edges.length()-1;
}


int Geometry::addTriangle(int n1,int n2,int n3,float norm[3])
{
	Triangle T;
	T.node[0]=n1;
	T.node[1]=n2;
	T.node[2]=n3;
	if (norm) {
		T.normal.data[0]=norm[0];
		T.normal.data[1]=norm[1];
		T.normal.data[2]=norm[2];
	} else {
		T.normal.zero();
		
	}
	triangles.append(T);

	return triangles.length()-1;
}

int Geometry::addCircle(const CoordinateSystem<float> XYZ,float radius)
{
	Circle T;
	T.XYZ=XYZ;
	T.radius=radius;
	circles.append(T);
	return circles.length()-1;
}

int Geometry::addArc(const CoordinateSystem<float> XYZ,float radius,float fmin,float fmax)
{
	ArcCircle T;
	T.XYZ=XYZ;
	T.radius=radius;
	T.fmin=fmin;
	T.fmax=fmax;
	arcs.append(T);
	return arcs.length()-1;
}

int Geometry::addSpline(float Px[4],float Py[4],float Pz[4])
{
	Spline T;
	memcpy(T.Px,Px,sizeof(T.Px));
	memcpy(T.Py,Py,sizeof(T.Py));
	memcpy(T.Pz,Pz,sizeof(T.Pz));
	splines.append(T);
	return splines.length()-1;
}

int Geometry::addBSpline(const BSpline &BS)
{
	bsplines.append(BS);
	return bsplines.length()-1;
}

int Geometry::addBSplineSurf(const BSplineSurf &BSS)
{
	bsplinesurfs.append(BSS);
	return bsplinesurfs.length()-1;
}

void Geometry::calcTrianglesNormals()
{
        unsigned int k;
	Triangle *tria;
	float *crd[3];
	
	vector3d<float> vec[3];	
	vector3d<float> normal_t[3];
	float normal_s[3]={0};

	float s;
	for (k=0; k<triangles.length(); k++) {
                tria=&triangles.at(k);

                crd[0]=grids.at(tria->node[0]).coords;
                crd[1]=grids.at(tria->node[1]).coords;
                crd[2]=grids.at(tria->node[2]).coords;

		vec[0].diff(crd[0],crd[1]);
		vec[1].diff(crd[1],crd[2]);
		vec[2].diff(crd[2],crd[0]);

		normal_t[0].cross_product(vec[0],vec[1]);
		normal_t[1].cross_product(vec[1],vec[2]);
		normal_t[2].cross_product(vec[2],vec[0]);

		normal_s[0]=normal_t[0].dot_product(normal_t[0]);
		normal_s[1]=normal_t[1].dot_product(normal_t[1]);
		normal_s[2]=normal_t[2].dot_product(normal_t[2]);

		int biggest_normal;

		if (normal_s[0]>=normal_s[1] && normal_s[0]>=normal_s[2]) {
			biggest_normal=0;
		} else if (normal_s[1]>=normal_s[2] && normal_s[1]>=normal_s[0]) {
			biggest_normal=1;
		} else {
			biggest_normal=2;
		}

		s=sqrtf(normal_s[biggest_normal]);
		tria->normal.copy(normal_t[biggest_normal]);
		tria->normal.scale(1./s);

	}
}

void Geometry::calcTrianglesSmoothNormals()
{
	if (hasSmoothNormals) return;
	hasSmoothNormals=1;

	Triangle *tria;

        unsigned int k,k1;
	/*Calculating smooth normals algorithm*/
	clock_t t=clock();

	/*First, how many triangles are attached on each grid*/
        unsigned int *trianglesPerGrid=(unsigned int *)calloc(grids.length(),sizeof(unsigned int));

	int globalTrianglesPerGrid=0;

	for (k=0; k<triangles.length(); k++) {
                tria=&triangles.at(k);
		for (k1=0; k1<3; k1++) {
			trianglesPerGrid[tria->node[k1]]++;
		}
	}

	for (k=0; k<grids.length(); k++) {
		globalTrianglesPerGrid+=trianglesPerGrid[k];
	}

	int **trianglesOnGrid=new int *[grids.length()];

	int *globalTrianglesOnGrid=new int[globalTrianglesPerGrid];

	globalTrianglesPerGrid=0;
	for (k=0; k<grids.length(); k++) {
		trianglesOnGrid[k]=&globalTrianglesOnGrid[globalTrianglesPerGrid];
		globalTrianglesPerGrid+=trianglesPerGrid[k];
		trianglesPerGrid[k]=0;
	}

	for (k=0; k<triangles.length(); k++) {
                tria=&triangles.at(k);
		for (k1=0; k1<3; k1++) {
			trianglesOnGrid[tria->node[k1]][trianglesPerGrid[tria->node[k1]]]=k;
			trianglesPerGrid[tria->node[k1]]++;
		}
	}

	vector3d<float> cnormal;

	for (k=0; k<grids.length(); k++) {
		cnormal.zero();
		for (k1=0; k1<trianglesPerGrid[k]; k1++) {
                        tria=&triangles.at(trianglesOnGrid[k][k1]);
			cnormal.add(tria->normal);
		}
		float *norm=cnormal.data;
		float s=sqrtf(norm[0]*norm[0]+norm[1]*norm[1]+norm[2]*norm[2]);
		cnormal.scale(1./s);
		

		int j;
		for (k1=0; k1<trianglesPerGrid[k]; k1++) {
                        tria=&triangles.at(trianglesOnGrid[k][k1]);
			j=-1;
			if (tria->node[0]==k) j=0;
			else if (tria->node[1]==k) j=1;
			else if (tria->node[2]==k) j=2;
			if (j!=-1) {
				tria->cnormal[j].copy(cnormal);
			}
		}
	}

	delete []globalTrianglesOnGrid;
	delete []trianglesOnGrid;

	

	free(trianglesPerGrid);

	qDebug("Time to calcTrianglesSmoothNormals: %f msec",(clock()-t)/(CLOCKS_PER_SEC/1000.));


}

void Geometry::translateGeometry(float mat[4][4])
{
	int k;
	float v[3],*v1;
	for (k=0; k<grids.length(); k++) {
                v1=grids.at(k).coords;
		v[0]=mat[0][0]*v1[0]+mat[0][1]*v1[1]+mat[0][2]*v1[2]+mat[0][3];
		v[1]=mat[1][0]*v1[0]+mat[1][1]*v1[1]+mat[1][2]*v1[2]+mat[1][3];
		v[2]=mat[2][0]*v1[0]+mat[2][1]*v1[1]+mat[2][2]*v1[2]+mat[2][3];
		v1[0]=v[0]; v1[1]=v[1]; v1[2]=v[2];
	}
}

typedef struct {
	int nod[2];
	int elem[2];
}Edge;


static int compareEdge(const void *f1,const void *f2)
{
	if (((Edge *)f1)->nod[0]>((Edge *)f2)->nod[0]) return 1;
	else if (((Edge *)f1)->nod[0]<((Edge *)f2)->nod[0]) return -1;
	else {
		if (((Edge *)f1)->nod[1]>((Edge *)f2)->nod[1]) return 1;
		else if (((Edge *)f1)->nod[1]<((Edge *)f2)->nod[1]) return -1;
		else return 0;
	}
}



void Geometry::recalcEdge(float angle)
{
	float cf=cos(angle);

	/*Edge calculation*/
	int k;

	int edge_len=3*triangles.length();
	Edge *edge=(Edge *)malloc(edge_len*sizeof(Edge));
	edge_len=0;

	for (k=0; k<triangles.length(); k++) {
                edge[edge_len].nod[0]=triangles.at(k).node[0];
                edge[edge_len].nod[1]=triangles.at(k).node[1];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;

                edge[edge_len].nod[0]=triangles.at(k).node[1];
                edge[edge_len].nod[1]=triangles.at(k).node[2];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;

                edge[edge_len].nod[0]=triangles.at(k).node[2];
                edge[edge_len].nod[1]=triangles.at(k).node[0];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;
	}
	int rp;

	if (edge_len) {

		for (k=0; k<edge_len; k++) {
			if (edge[k].nod[0]>edge[k].nod[1]) {
				rp=edge[k].nod[0];
				edge[k].nod[0]=edge[k].nod[1];
				edge[k].nod[1]=rp;
			}
		}

		qsort(edge,edge_len,sizeof(Edge),compareEdge);
		rp=0;
		for (k=1; k<edge_len; k++) {
			if (edge[k].nod[0]==edge[rp].nod[0] &&
				edge[k].nod[1]==edge[rp].nod[1]) {
					edge[rp].elem[1]=edge[k].elem[0];
			} else {
				rp++;
				edge[rp].nod[0]=edge[k].nod[0];
				edge[rp].nod[1]=edge[k].nod[1];
				edge[rp].elem[0]=edge[k].elem[0];
				edge[rp].elem[1]=edge[k].elem[1];
			}
		}

		edge_len=rp+1;
		float *nrm1,*nrm2;
		float cosf;
		for (k=0; k<edge_len; k++) {
			if (edge[k].elem[0]!=-1 && edge[k].elem[1]!=-1) {



                                nrm1=triangles.at(edge[k].elem[0]).normal.data;
                                nrm2=triangles.at(edge[k].elem[1]).normal.data;
				
				cosf=nrm1[0]*nrm2[0]+nrm1[1]*nrm2[1]+nrm1[2]*nrm2[2];
				if (cosf>-cf && cosf<cf ) {
					addEdge(edge[k].nod[0],edge[k].nod[1]);
				}
			} else if (edge[k].elem[0]!=-1) {
				addEdge(edge[k].nod[0],edge[k].nod[1]);
			}
		}
	}

	free(edge);

}


static int compareGrids(const Grid *f1,const Grid *f2)
{
	if (f1->coords[0]>f2->coords[0]) return 1;
	else if (f1->coords[0]<f2->coords[0]) return -1;
	else {
		if (f1->coords[1]>f2->coords[1]) return 1;
		else if (f1->coords[1]<f2->coords[1]) return -1;
		else {
			if (f1->coords[2]>f2->coords[2]) return 1;
			else if (f1->coords[2]<f2->coords[2]) return -1;
			else return 0;
		}
	}
}

static int compareGrids1(const Grid *f1,const Grid *f2)
{
	if (f1->coords[1]>f2->coords[1]) return 1;
	else if (f1->coords[1]<f2->coords[1]) return -1;
	else {
		if (f1->coords[2]>f2->coords[2]) return 1;
		else if (f1->coords[2]<f2->coords[2]) return -1;
		else {
			if (f1->coords[0]>f2->coords[0]) return 1;
			else if (f1->coords[0]<f2->coords[0]) return -1;
			else return 0;
		}
	}
}


void Geometry::shrinkGeometry()
{
	int k;
	/*Must move whole model into [-5,0,0],[5,10,5]*/
	float r1=maxx[0]-minn[0];
	float r2=maxx[1]-minn[1];
	float r3=maxx[2]-minn[2];

	r1=10./r1; r2=10./r2; r3=5./r3;
	float r=r1;
	if (r2<r) r=r2;
	if (r3<r) r=r3;

	for (k=0; k<grids.length(); k++) {
                grids.at(k).coords[0]-=(minn[0]+maxx[0])*.5;
                grids.at(k).coords[1]-=(minn[1]+maxx[1])*.5;
                grids.at(k).coords[2]-=(minn[2]+maxx[2])*.5;

		
                grids.at(k).coords[0]*=r;
                grids.at(k).coords[1]*=r;
                grids.at(k).coords[2]*=r;

                grids.at(k).coords[1]+=5;
                grids.at(k).coords[2]+=2.5;
	}
}


void Geometry::compressGrids()
{
	if (!grids.length()) return;
	float dx,dy,dz;
	int *realPos;
	int rp;
	int k;

	/*Sorting grids(012)*/
	grids.Qsort(compareGrids);
	
	realPos=new int[grids.length()];
	for (k=0; k<grids.length(); k++) {
                realPos[grids.at(k).pos]=k;
	}

	/*Converting triangle grids*/
	for (k=0; k<triangles.length(); k++) {
		triangles.at(k).node[0]=realPos[triangles.at(k).node[0]];
		triangles.at(k).node[1]=realPos[triangles.at(k).node[1]];
		triangles.at(k).node[2]=realPos[triangles.at(k).node[2]];
	}
	/*Converting line grids*/
	for (k=0; k<lines.length(); k++) {
		lines.at(k).node[0]=realPos[lines.at(k).node[0]];
		lines.at(k).node[1]=realPos[lines.at(k).node[1]];
	}
	/*Converting point grids*/
	for (k=0; k<points.length(); k++) {
		points.at(k)=realPos[points.at(k)];
	}
	/*Converting edge grids*/
	for (k=0; k<edges.length(); k++) {
		edges.at(k).node[0]=realPos[edges.at(k).node[0]];
		edges.at(k).node[1]=realPos[edges.at(k).node[1]];
	}



	for (k=0; k<grids.length(); k++) grids.at(k).pos=k;

	realPos[0]=0;
	rp=0;
	for (k=1; k<grids.length(); k++) {
		dx=grids.at(k).coords[0]-grids.at(rp).coords[0];
		dy=grids.at(k).coords[1]-grids.at(rp).coords[1];
		dz=grids.at(k).coords[2]-grids.at(rp).coords[2];
		if (dx*dx+dy*dy+dz*dz<1e-12)
		{
			dx=0;
		} else {
			rp++;
			grids.at(rp).coords[0]=grids.at(k).coords[0];
			grids.at(rp).coords[1]=grids.at(k).coords[1];
			grids.at(rp).coords[2]=grids.at(k).coords[2];
		}
		realPos[k]=rp;
	}
	grids.truncateInto(rp+1);

	/*Converting triangle grids*/
	for (k=0; k<triangles.length(); k++) {
		triangles.at(k).node[0]=realPos[triangles.at(k).node[0]];
		triangles.at(k).node[1]=realPos[triangles.at(k).node[1]];
		triangles.at(k).node[2]=realPos[triangles.at(k).node[2]];
	}
	/*Converting line grids*/
	for (k=0; k<lines.length(); k++) {
		lines.at(k).node[0]=realPos[lines.at(k).node[0]];
		lines.at(k).node[1]=realPos[lines.at(k).node[1]];
	}
	/*Converting point grids*/
	for (k=0; k<points.length(); k++) {
		points.at(k)=realPos[points.at(k)];
	}
	/*Converting edge grids*/
	for (k=0; k<edges.length(); k++) {
		edges.at(k).node[0]=realPos[edges.at(k).node[0]];
		edges.at(k).node[1]=realPos[edges.at(k).node[1]];
	}
	delete []realPos;
}

void Geometry::loadSTL(char *name)
{
	readSTL(this,name);
		
	//shrinkGeometry();
	
	compressGrids();

	calcTrianglesNormals();

	recalcEdge(30*3.14159/180.);

	makeEdgeStrip();
	makeLineStrip();
	makeTriaStrip();
}



void Geometry::loadDXF(char *name)
{

	readDXF(this,name);

	//shrinkGeometry();

	compressGrids();

	calcTrianglesNormals();

	makeEdgeStrip();
	makeLineStrip();
	makeTriaStrip();
}






void Geometry::load3DS(char *name)
{
	readChunck3DS(this,name);
	

	shrinkGeometry();
	calcTrianglesNormals();

	calcTrianglesSmoothNormals();

	makeEdgeStrip();
	makeLineStrip();
	makeTriaStrip();
}


void Geometry::loadIGES(char *name)
{
	readIGES(this,name);

	compressGrids();

	calcTrianglesNormals();

	makeEdgeStrip();
	makeLineStrip();
	makeTriaStrip();

}


void Geometry::makeEdgeStrip()
{

	if (edges.length()==0) return;

	clock_t t=clock();

	int k,k1,k2;
	int *totalEdgesOnGrid=(int *)calloc(grids.length(),sizeof(int));
	for (k=0; k<edges.length(); k++) {
		totalEdgesOnGrid[edges.at(k).node[0]]++;
		totalEdgesOnGrid[edges.at(k).node[1]]++;
	}
	
	
	int *conn_buffer=(int *)calloc(2*edges.length(),sizeof(int));
	int **edgesOnGrid=(int **)calloc(grids.length(),sizeof(int *));
	
	edgesOnGrid[0]=conn_buffer;
	for (k=1; k<grids.length(); k++) {
		edgesOnGrid[k]=edgesOnGrid[k-1]+ totalEdgesOnGrid[k-1];
		totalEdgesOnGrid[k-1]=0;
	}	
	totalEdgesOnGrid[grids.length()-1]=0;

	int edj1,edj2,fnd;

	for (k=0; k<edges.length(); k++) {
		edj1=edges.at(k).node[0];
		edj2=edges.at(k).node[1];

		edgesOnGrid[edj1][totalEdgesOnGrid[edj1]]=edj2;
		totalEdgesOnGrid[edj1]++;
		edgesOnGrid[edj2][totalEdgesOnGrid[edj2]]=edj1;
		totalEdgesOnGrid[edj2]++;
		
	}

	int ARR[200];
	int arr_pos;
	k=0;

	free(edgeStrip);
	int edgeStripLen,edgeStripMem;
	edgeStripMem=1024;
	edgeStripLen=0;
	edgeStrip=(int *)malloc(edgeStripMem*sizeof(int));

	int k0;
	k0=0;
	for (;;) {
		while (k0<grids.length() && totalEdgesOnGrid[k0]==0) {
			k0++;
		}
		if (k0==grids.length()) break;

		k=k0;
		
		arr_pos=0;

		ARR[arr_pos]=k;
		arr_pos++;

		

		while (arr_pos<150) {
			totalEdgesOnGrid[k]--;
			k1=edgesOnGrid[k][totalEdgesOnGrid[k]];
			
			int fnd=0;
			
			for (k2=0; k2<totalEdgesOnGrid[k1]; k2++) {
				if (!fnd) {
					if (edgesOnGrid[k1][k2]==k) fnd=1;
				} else {
					edgesOnGrid[k1][k2-1]=edgesOnGrid[k1][k2];
				}
			}
			totalEdgesOnGrid[k1]--;

			k=k1;

			ARR[arr_pos]=k;
			arr_pos++;

			if (totalEdgesOnGrid[k]==0) {
				break;
			}
		}
		fnd=0;
		while (edgeStripMem<edgeStripLen+arr_pos+1) {
			edgeStripMem*=2;
			fnd=1;
		}
		if (fnd) {
			edgeStrip=(int *)realloc(edgeStrip,edgeStripMem*sizeof(int));
		}

		edgeStrip[edgeStripLen]=arr_pos;
		edgeStripLen++;

		for (k=0; k<arr_pos; k++) {
			edgeStrip[edgeStripLen]=ARR[k];
			edgeStripLen++;
		}

	}
	if (edgeStripMem==edgeStripLen) {
		edgeStripMem+=1;
		edgeStrip=(int *)realloc(edgeStrip,edgeStripMem*sizeof(int));
	}
	edgeStrip[edgeStripLen]=0;
	edgeStripLen++;
	
	free(totalEdgesOnGrid);
	free(edgesOnGrid);
	free(conn_buffer);


	qDebug("Time to edge strip: %f msec",(clock()-t)/(CLOCKS_PER_SEC/1000.));
}

void Geometry::makeLineStrip()
{

	if (lines.length()==0) return;
	

	clock_t t=clock();

	int k,k1,k2;
	int *totalLinesOnGrid=(int *)calloc(grids.length(),sizeof(int));
	for (k=0; k<lines.length(); k++) {
		totalLinesOnGrid[lines.at(k).node[0]]++;
		totalLinesOnGrid[lines.at(k).node[1]]++;
	}
	
	
	int *conn_buffer=(int *)calloc(2*lines.length(),sizeof(int));
	int **linesOnGrid=(int **)calloc(grids.length(),sizeof(int *));
	
	linesOnGrid[0]=conn_buffer;
	for (k=1; k<grids.length(); k++) {
		linesOnGrid[k]=linesOnGrid[k-1]+ totalLinesOnGrid[k-1];
		totalLinesOnGrid[k-1]=0;
	}	
	totalLinesOnGrid[grids.length()-1]=0;

	int edj1,edj2,fnd;

	for (k=0; k<lines.length(); k++) {
		edj1=lines.at(k).node[0];
		edj2=lines.at(k).node[1];

		linesOnGrid[edj1][totalLinesOnGrid[edj1]]=edj2;
		totalLinesOnGrid[edj1]++;
		linesOnGrid[edj2][totalLinesOnGrid[edj2]]=edj1;
		totalLinesOnGrid[edj2]++;
		
	}

	int ARR[200];
	int arr_pos;
	k=0;

	free(lineStrip);
	int lineStripLen,lineStripMem;
	lineStripMem=1024;
	lineStripLen=0;
	lineStrip=(int *)malloc(lineStripMem*sizeof(int));

	int k0;
	k0=0;
	for (;;) {
		while (k0<grids.length() && totalLinesOnGrid[k0]==0) {
			k0++;
		}
		if (k0==grids.length()) break;

		k=k0;
		
		arr_pos=0;

		ARR[arr_pos]=k;
		arr_pos++;

		

		while (arr_pos<150) {
			totalLinesOnGrid[k]--;
			k1=linesOnGrid[k][totalLinesOnGrid[k]];
			
			int fnd=0;
			
			for (k2=0; k2<totalLinesOnGrid[k1]; k2++) {
				if (!fnd) {
					if (linesOnGrid[k1][k2]==k) fnd=1;
				} else {
					linesOnGrid[k1][k2-1]=linesOnGrid[k1][k2];
				}
			}
			totalLinesOnGrid[k1]--;

			k=k1;

			ARR[arr_pos]=k;
			arr_pos++;

			if (totalLinesOnGrid[k]==0) {
				break;
			}
		}
		fnd=0;
		while (lineStripMem<lineStripLen+arr_pos+1) {
			lineStripMem*=2;
			fnd=1;
		}
		if (fnd) {
			lineStrip=(int *)realloc(lineStrip,lineStripMem*sizeof(int));
		}

		lineStrip[lineStripLen]=arr_pos;
		lineStripLen++;

		for (k=0; k<arr_pos; k++) {
			lineStrip[lineStripLen]=ARR[k];
			lineStripLen++;
		}

	}
	if (lineStripMem==lineStripLen) {
		lineStripMem+=1;
		lineStrip=(int *)realloc(lineStrip,lineStripMem*sizeof(int));
	}
	lineStrip[lineStripLen]=0;
	lineStripLen++;
	
	free(totalLinesOnGrid);
	free(linesOnGrid);
	free(conn_buffer);


	qDebug("Time to line strip: %f msec",(clock()-t)/(CLOCKS_PER_SEC/1000.));
}


void Geometry::makeTriaStrip()
{
	return;
	/*TODO Not ready yet*/
	clock_t t=clock();
	free(triaStripVertex); triaStripVertex=0;
	free(triaStripElement); triaStripElement=0;


	/*Edge calculation*/
	int k,rp;
	int edge_len=3*triangles.length();
	Edge *edge=(Edge *)malloc(edge_len*sizeof(Edge));
	
	edge_len=0;	

	for (k=0; k<triangles.length(); k++) {
		edge[edge_len].nod[0]=triangles.at(k).node[0];
		edge[edge_len].nod[1]=triangles.at(k).node[1];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;

		edge[edge_len].nod[0]=triangles.at(k).node[1];
		edge[edge_len].nod[1]=triangles.at(k).node[2];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;

		edge[edge_len].nod[0]=triangles.at(k).node[2];
		edge[edge_len].nod[1]=triangles.at(k).node[0];
		edge[edge_len].elem[0]=k;
		edge[edge_len].elem[1]=-1;
		edge_len++;
	}

	for (k=0; k<edge_len; k++) {
		if (edge[k].nod[0]>edge[k].nod[1]) {
			rp=edge[k].nod[0];
			edge[k].nod[0]=edge[k].nod[1];
			edge[k].nod[1]=rp;
		}
	}
	qsort(edge,edge_len,sizeof(Edge),compareEdge);
	rp=0;
	for (k=1; k<edge_len; k++) {
		if (edge[k].nod[0]==edge[rp].nod[0] &&
			edge[k].nod[1]==edge[rp].nod[1]) {
				edge[rp].elem[1]=edge[k].elem[0];
		} else {
			rp++;
			edge[rp].nod[0]=edge[k].nod[0];
			edge[rp].nod[1]=edge[k].nod[1];
			edge[rp].elem[0]=edge[k].elem[0];
			edge[rp].elem[1]=edge[k].elem[1];
		}
	}

	int ARR[200],ARR_E[200];
	
	int (*conn)[3];
	int *connLen;
	conn=(int (*)[3])malloc(triangles.length()*sizeof(int[3]));
	connLen=(int *)calloc(triangles.length(),sizeof(int));

	int el1,el2;
	for (k=0; k<edge_len; k++) {
		el1=edge[k].elem[0];
		el2=edge[k].elem[1];
		if (el1!=-1) {
			conn[el1][connLen[el1]]=el2;
			connLen[el1]++;
		}
		if (el2!=-1) {
			conn[el2][connLen[el2]]=el1;
			connLen[el2]++;
		}
	}

	int k0;
	k0=0;
	for (;;) {
		while (k0<triangles.length() && connLen[k0]==0) {
			k0++;
		}
		if (k0==triangles.length()) break;

		

	}

	free(connLen);
	free(conn);

	free(edge);

	qDebug("Time to triangle strip: %f msec",(clock()-t)/(CLOCKS_PER_SEC/1000.));
}


void Geometry::drawEdgeStrip()
{

	glColor4fv(edgeStripColor);

	if (!edgeStrip) {
		int k;
		glBegin(GL_LINES);
		for (k=0; k<edges.length(); k++) {
			glVertex3fv(grids.at(edges.at(k).node[0]).coords);
			glVertex3fv(grids.at(edges.at(k).node[1]).coords);
		}
		glEnd();
	} else {
		int *ar,totta;
		ar=edgeStrip;
#if 0


		while (ar[0]) {
			totta=ar[0]; ar++;
			glBegin(GL_LINE_STRIP);
			for (k=0; k<totta; k++) {
				glVertex3fv(grids.data[ar[k]].coords);
			}
			glEnd();
			ar+=totta;
		}
#else
		glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3,GL_FLOAT,sizeof(Grid),&grids.at(0).coords);

		while (ar[0]) {
			totta=ar[0]; ar++;
			glDrawElements(GL_LINE_STRIP,totta,GL_UNSIGNED_INT,ar);
			ar+=totta;
		}

		glDisableClientState(GL_VERTEX_ARRAY);

#endif
	}

}

void Geometry::drawLineStrip()
{
	glColor4fv(lineStripColor);

	if (!lineStrip) {
		int k;

		glBegin(GL_LINES);
		for (k=0; k<lines.length(); k++) {
			glVertex3fv(grids.at(lines.at(k).node[0]).coords);
			glVertex3fv(grids.at(lines.at(k).node[1]).coords);
		}
		glEnd();
	} else {
		int *ar,totta;
		ar=lineStrip;
#if 0


		while (ar[0]) {
			totta=ar[0]; ar++;
			glBegin(GL_LINE_STRIP);
			for (k=0; k<totta; k++) {
				glVertex3fv(grids.data[ar[k]].coords);
			}
			glEnd();
			ar+=totta;
		}
#else
		glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3,GL_FLOAT,sizeof(Grid),&grids.at(0).coords);



		while (ar[0]) {
			totta=ar[0]; ar++;
			glDrawElements(GL_LINE_STRIP,totta,GL_UNSIGNED_INT,ar);
			ar+=totta;
		}

		glDisableClientState(GL_VERTEX_ARRAY);
#endif
	}


}


void Geometry::drawCircles()
{
	glColor4fv(lineStripColor);

	
	int i;
	float f;
	float X0[3];
	float X[3];
	float fmin,fmax;
	float df;
	fmin=0; fmax=2*3.14159;
	df=fmax/50.;
	for (i=0; i<circles.length(); i++) {
		const Circle & C=circles.at(i);
		glBegin(GL_LINE_LOOP);
		for (f=fmin; f<fmax; f+=df) {
			X0[0]=C.radius*cos(f);
			X0[1]=C.radius*sin(f);
			X0[2]=0;
			C.XYZ.fromLocalToGlobal(X,X0);
			glVertex3fv(X);
		}
		glEnd();
	}

}

void Geometry::drawArcs()
{
	glColor4fv(lineStripColor);

	int i;
	float f;
	float X0[3];
	float X[3];
	float df;
	df=(2*3.14159)/50.;
	for (i=0; i<arcs.length(); i++) {
		const ArcCircle & C=arcs.at(i);
		glBegin(GL_LINE_STRIP);
		
		for (f=C.fmin; f<C.fmax; f+=df) {
			X0[0]=C.radius*cos(f);
			X0[1]=C.radius*sin(f);
			X0[2]=0;
			C.XYZ.fromLocalToGlobal(X,X0);
			glVertex3fv(X);
		}
		f=C.fmax;
		X0[0]=C.radius*cos(f);
		X0[1]=C.radius*sin(f);
		X0[2]=0;
		C.XYZ.fromLocalToGlobal(X,X0);
		glVertex3fv(X);
		glEnd();
	}

}

void Geometry::drawSplines()
{
	glColor4fv(lineStripColor);
	int i;
	float s;
	float s2,s3;
	float ds=0.01;
	float X[3];
	for (i=0; i<splines.length(); i++) {
		const Spline & S=splines.at(i);
		glBegin(GL_LINE_STRIP);
		for (s=0; s<=1; s+=ds) {
			s2=s*s;
			s3=s2*s;
			X[0]=S.Px[0]+S.Px[1]*s+S.Px[2]*s2+S.Px[3]*s3;
			X[1]=S.Py[0]+S.Py[1]*s+S.Py[2]*s2+S.Py[3]*s3;
			X[2]=S.Pz[0]+S.Pz[1]*s+S.Pz[2]*s2+S.Pz[3]*s3;
			glVertex3fv(X);
		}
		glEnd();
	}
}

void Geometry::drawBSplines()
{
	glColor4fv(lineStripColor);
	int i,j;
	float t;
	float dt=0.05;
	float X[3];
	for (i=0; i<bsplines.length(); i++) {
		const BSpline & BS=bsplines.at(i);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3,GL_FLOAT,0,BS.coords);

		int *ar,totta;

		ar=BS.strip;

		while (ar[0]) {
			totta=ar[0]; ar++;
			glDrawElements(GL_LINE_STRIP,totta,GL_UNSIGNED_INT,ar);
			ar+=totta;
		}

		glDisableClientState(GL_VERTEX_ARRAY);

	}
}

void Geometry::drawBSplineSurfs()
{
	glShadeModel(GL_SMOOTH);
	int i;
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	for (i=0; i<bsplinesurfs.length(); i++) {
		const BSplineSurf &BSS=bsplinesurfs.at(i);
		glVertexPointer(3,GL_FLOAT,0,BSS.coords);
		glNormalPointer(GL_FLOAT,0,BSS.normals);

		int *ar,totta;

		ar=BSS.strip;

		while (ar[0]) {
			totta=ar[0]; ar++;
			glDrawElements(GL_TRIANGLE_STRIP,totta,GL_UNSIGNED_INT,ar);
			ar+=totta;
		}

	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glShadeModel(GL_FLAT);
}


void Geometry::drawRevolveLines()
{
	glShadeModel(GL_SMOOTH);
	int i;
	for (i=0; i<revolvelines.length(); i++) {
		const RevolveLine &RL=revolvelines.at(i);
		const Line &axisL=lines.at(RL.line_axis_pos);
		const Line &genL=lines.at(RL.line_gen_pos);

		float axis[2][3],Z[3];
		float gen[2][3];
		vec_copy(axis[0],grids.at( axisL.node[0] ).coords);
		vec_copy(axis[1],grids.at( axisL.node[1] ).coords);
		vec_diff(Z,axis[1],axis[0]);

		vec_copy(gen[0],grids.at( genL.node[0] ).coords);
		vec_copy(gen[1],grids.at( genL.node[1] ).coords);

		float X[2][3],Y[2][3],O[2][3];
		float r[2];

		project_on_line(O[0],gen[0],axis[0],axis[1]);
		project_on_line(O[1],gen[1],axis[0],axis[1]);

		vec_diff(X[0],gen[0],O[0]);
		vec_length(&r[0],X[0]);
		vec_cross_product(Y[0],Z,X[0]);
		vec_normalize(X[0]);
		vec_normalize(Y[0]);

		vec_diff(X[1],gen[1],O[1]); 
		vec_length(&r[1],X[1]);
		vec_cross_product(Y[1],Z,X[1]);
		vec_normalize(X[1]);
		vec_normalize(Y[1]);

		float f,df;
		df=(RL.fmax-RL.fmin)/50.;

		glBegin(GL_QUAD_STRIP);
		f=RL.fmin;
		for (;;) {
			float cosf,sinf;
			float n[2][3];
			float s[2][3];
			float tmp[3],tmp1[3],tmp2[3];

			cosf=cos(f); sinf=sin(f);

			vec_scale(tmp,cosf,X[0]);
			vec_copy(n[0],tmp);
			vec_scale(tmp,sinf,Y[0]);
			vec_sum(n[0],n[0],tmp);

			vec_scale(tmp,cosf,X[1]);
			vec_copy(n[1],tmp);
			vec_scale(tmp,sinf,Y[1]);
			vec_sum(n[1],n[1],tmp);

			glNormal3fv(n[1]);

			vec_scale(s[0],r[0],n[0]);
			vec_scale(s[1],r[1],n[1]);


			vec_sum(s[0],s[0],O[0]);
			vec_sum(s[1],s[1],O[1]);


			vec_diff(tmp,s[0],s[1]);

			vec_cross_product(tmp1,n[0],Z);
			vec_cross_product(tmp2,tmp,tmp1);
			vec_normalize(tmp2);
			glNormal3fv(tmp2);
			glVertex3fv(s[0]);

			vec_cross_product(tmp1,n[1],Z);
			vec_cross_product(tmp2,tmp,tmp1);
			vec_normalize(tmp2);
			glNormal3fv(tmp2);
			glVertex3fv(s[1]);

			if (f==RL.fmax) break;

			f+=df;
			if (f>RL.fmax) {
				f=RL.fmax;
			}
		}

		glEnd();

	}
	glShadeModel(GL_FLAT);
}

