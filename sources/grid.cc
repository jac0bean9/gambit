//#
//# FILE: solve.cc
//#

#include "rational.h"
#include "nfrep.h"
#include "normal.h"
#include "normiter.h"
#include "probvect.h"
#include "equdata.h"
#include "stpwatch.h"

#define SCREEN_OUTPUT		// Controls if solutions are displayed on gout as they are obtained

template <class T> class GridSolveModule   {
	private:
		NFRep<T> &rep;
		FileHeader header;
		gVector<T> p,x,q_calc,y;
		gOutput &out;

		void  (*update_func)(void);
		int CheckEqu(gVector<T> &q,T l);
		void OutputResult(gOutput &out,T l,T dist,gVector<T> &q,gVector<T> &p);
	public:
		GridSolveModule(NFRep<T> &r,gInput &in,gOutput &out);
		GridSolveModule(NFRep<T> &r,gOutput &out,T _merror=-1.0,T _qstep=-1.0,
		T _estep=-1.0,T _estart=-1.0,T _estop=-1.0,int _data_type=-1);
		void	SetUpdateFunc(void (*upd)(void)) {update_func=upd;}
//
// Finds the equilibria points at a given error value using the grid search
//
		int GridSolve(void);

};

template <class T> GridSolveModule<T>::GridSolveModule(NFRep<T> &r,gInput &params,gOutput &_out)
: rep(r), header(params),out(_out),x(r.NumStrats(1)),p(r.NumStrats(1)),y(r.NumStrats(2)),q_calc(r.NumStrats(2)),
update_func(NULL)
{}

template <class T> GridSolveModule<T>::GridSolveModule(NFRep<T> &r,gOutput &_out,
										T _merror,T _qstep,T _estep,T _estart,T _estop,int _data_type)
: rep(r),out(_out),x(r.NumStrats(1)),p(r.NumStrats(1)),y(r.NumStrats(2)),q_calc(r.NumStrats(2))
{
gBlock<int> strategies(r.NumPlayers());
for(int i=1;i<=r.NumPlayers();i++) strategies[i]=r.NumStrats(i);
header=FileHeader(_merror,_qstep,_estep,_estart,_estop,_data_type,r.NumPlayers(),strategies);
}

template <class T> GridSolveModule<T>::~GridSolveModule()
{}

template <class T> void GridSolveModule<T>::OutputResult(gOutput &out,T l,T dist,gVector<T> &q,gVector<T> &p)
{
int	i;
for (i = p.First(); i <= p.Last(); i++)  out << p[i] << ' ';
for (i = q.First(); i <= q.Last(); i++)  out << q[i] << ' ';
out<<l<<' '<<dist<<'\n';
}

template <class T> int GridSolveModule<T>::CheckEqu(gVector<T> &q,T l)
{
T 		denom;					// denominator of function
Bool  ok; 	          // was a solution found?
T 		dist;						// metric function--distance from the equ
int   i,j;

/*---------------------make X's---------------------*/
for (i=1;i<=header.NumStrategies(1);i++)
{
		x[i]=0;
		for (j=1;j<=header.NumStrategies(2);j++)	x[i]+=((T)(*(header.Matrix()))(i-1,j-1).row)*q[j];
}
/*--------------------make P's----------------------*/
denom=0;
for (i=1;i<=header.NumStrategies(1);i++) denom+=exp(l*x[i]);
for (i=1;i<=header.NumStrategies(1);i++) p[i]=((T)exp(l*x[i]))/denom;
/*--------------------make Y's----------------------*/
for (i=1;i<=header.NumStrategies(2);i++)
{
	y[i]=0.0;
	for (j=1;j<=header.NumStrategies(1);j++)	y[i]+=((T)(*(header.Matrix()))(j-1,i-1).col)*p[j];
}
/*--------------------make Q_CALC's-----------------*/
denom=0.0;
for (i=1;i<=header.NumStrategies(2);i++) denom+=exp(l*y[i]);
for (i=1;i<=header.NumStrategies(2);i++) q_calc[i]=((T)exp(l*y[i]))/denom;
/*--------------------check for equilibrium---------*/
			 /* Note: this uses the very crude method for finding*
			 * if two points are 'close enough.'  The correspon-*
			 * ding coordinates of each point are subtracted and*
			 * abs or result is compared to MERROR              *
			 * A more precise way would be to use the Distance  *
			 * function provided, but that would be very slow   *
			 * if Distance is not used, value of ok is either   *
			 * 1.0 or 0.0                                       */

ok=TRUE;dist=0;
for (i=1;i<=header.NumStrategies(1);i++)
{
	dist+=fabs((double)(q[i]-q_calc[i]));
	if ((fabs((double)(q[i]-q_calc[i]))>=header.MError())) ok=FALSE;
}


if (ok)	OutputResult(out,l,dist,q,p);

return (ok);
}


template <class T> int GridSolveModule<T>::GridSolve(void)
{
int i,j;
NormalIter<T> iter(rep);
int num_infosets=rep.NumPlayers();
int	dim_x=rep.NumStrats(1),dim_y=rep.NumStrats(2);
assert(num_infosets==2);
// Build a FileHeader from scratch.....
header.SetNumInfosets(rep.NumPlayers());
for (i=1;i<=rep.NumPlayers();i++) header.SetNumStrategies(i,rep.NumStrats(i));
header.SetMatrix();		// Create a copy of the matrix in the FileHeader
for (j=1;j<=dim_y;j++)
	for (i=1;i<=dim_x;i++)
	{
		iter.Set(1,j);iter.Set(2,i);
		header.SetMatrixValue(j-1,i-1,1,iter.Evaluate(1));
		header.SetMatrixValue(j-1,i-1,2,iter.Evaluate(2));
	}


// Initialize the output file
StopWatch *timer=new StopWatch;timer->Start();
out<<header;
// Create the ProbVector to give us all sets of probability values
ProbVect<T> *pv=new ProbVect<T>(header.NumStrategies(1),(int)(1.0/header.QStep()+0.5));

for (double l=header.EStart();l<header.EStop();l=(header.DataType()==DATA_TYPE_ARITH) ? (l+header.QStep()) : (l*header.QStep()))
{
	while (!pv->Done()) if (pv->Inc())	CheckEqu(pv->GetP(),T(l));
	pv->Reset();
	if (update_func) (*update_func)();
}
// Record the time taken and close the output file
out<<"Simulation took "<<timer->ElapsedStr()<<'\n';
delete timer;
delete pv;
return 1;
}

int NormalForm::GridSolve(const char *param_file,const char *out_file)
{
  gFileInput in(param_file);
  gFileOutput out(out_file);
  switch (type)
    {
    case nfDOUBLE:{
      GridSolveModule<double> M((NFRep<double> &) *data,in,out);
      M.GridSolve();
      return 1;
    }
    case nfRATIONAL:
      {
	GridSolveModule<Rational> M((NFRep<Rational> &) *data,in,out);
	M.GridSolve();
	return 1;
      }
    default:
      return 0;
    }
}
// Grid Solve, allows to set most parameters from the function call
int NormalForm::GridSolve(const char *out_file,double _merror,double _qstep,
double _estep,double _estart,double _estop,int _data_type)
{
gFileOutput out(out_file);
	switch (type)
	{
		case nfDOUBLE:{
			GridSolveModule<double> M((NFRep<double> &) *data,out,_merror,_qstep,
			_estep,_estart,_estop,_data_type);
			M.GridSolve();
			return 1;}
		case nfRATIONAL:
		{
			GridSolveModule<Rational> M((NFRep<Rational> &) *data,out,(Rational)_merror,(Rational)_qstep,
			(Rational)_estep,(Rational)_estart,(Rational)_estop,_data_type);
			M.GridSolve();
			return 1;
		}
		default:
       return 1;
	}
return 1;
}



