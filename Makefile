#---------------------------------DIR------------------------------------
GUROBIDIR= /opt/gurobi811/linux64
CPLEXDIR      = /opt/ibm/ILOG/CPLEX_Studio129/cplex
CONCERTDIR    = /opt/ibm/ILOG/CPLEX_Studio129/concert


#---------------------------------LIB--------------------------------------

CPLEXLIBDIR   = $(CPLEXDIR)/lib/x86-64_linux/static_pic
CONCERTLIBDIR = $(CONCERTDIR)/lib/x86-64_linux/static_pic
GUROBILIBDIR= $(GUROBIDIR)/lib

#---------------------------------LINK-------------------------------------
CPLEXINCDIR   = $(CPLEXDIR)/include
CONCERTINCDIR = $(CONCERTDIR)/include
CPLEXINC 	= -I$(CONCERTINCDIR) -I$(CPLEXINCDIR) 
CPLEXDIRS  	= -L$(CPLEXLIBDIR) -L$(CONCERTLIBDIR) 
CPLEXFLAGS 	= -lconcert -lilocplex -lcplex -lm -pthread 
GUROBILIB   = -L$(GUROBILIBDIR) -lgurobi_c++ -lgurobi81

#-------------------------------LPSOLVE------------------------------------------
lpsolveDIR=-L/usr/lib/lp_solve -llpsolve55

#------------------------------COMPILER-OPTION----------------------------------
CPP      = g++ -m64 -Ofast -fPIC -fno-strict-aliasing -fexceptions -DNDEBUG -DIL_STD -std=gnu++11
#--------------------------------------------------------------------------

CODE=rawfloorplanner
#CODE=switchFloorplanner
#--------------------------------------------------------------------------

ifeq ($(GRAPHIC_USE), 1)  
graphic=-lGL -lGLU -lglut -DGRAPHIC_USE
else
graphic=
endif



ifeq ($(ALL_USE), 1)
CPLEX_USE=1
GUROBI_USE=1
endif

ifeq ($(CPLEX_USE), 1)  
CPLEX =-DCPLEX_USE
else
CPLEX=
CPLEXINC=
CPLEXDIRS=
CPLEXFLAGS= 	
endif

ifeq ($(GUROBI_USE), 1)  
GUROBI =-DGUROBI_USE 
else
GUROBI =
GUROBILIB=
endif

LPSOLVE =
lpsolveDIR=


all:	
	$(CPP) $(LPSOLVE) $(GUROBI) $(CPLEX) $(CPLEXINC) $(CPLEXDIRS) -o $(CODE) $(CODE).cpp $(CPLEXFLAGS) $(GUROBILIB) -ldl $(lpsolveDIR)  $(graphic)

clean:
	rm -rf $(CODE)
