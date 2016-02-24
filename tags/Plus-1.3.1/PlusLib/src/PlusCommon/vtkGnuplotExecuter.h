/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

// .NAME vtkGnuplotExecuter - class for gnuplot execution 
// .SECTION Description
// This class is used for generating plots with gnuplot 

#ifndef __VTKGNUPLOTEXECUTER_H
#define __VTKGNUPLOTEXECUTER_H

#include "vtkObject.h"
#include "vtkTable.h"
#include <vector>


class VTK_EXPORT vtkGnuplotExecuter : public vtkObject
{
public: 
	static vtkGnuplotExecuter* New();
	vtkTypeRevisionMacro(vtkGnuplotExecuter,vtkObject);
	void PrintSelf(ostream& os, vtkIndent indent); 

	// Description:
	// Execute gnuplot with the given arguments
	PlusStatus Execute(); 

	// Description:
	// Add custom argument
	void AddArgument(const char* argument); 

	// Description:
	// Clear arguments 
	void ClearArguments(); 

  // Description:
	// Dump vtkTable to text file in gnuplot format
  static PlusStatus DumpTableToFileInGnuplotFormat( vtkTable* table, const char* filename); 

	// Description:
	// Set/get working directory
	vtkSetStringMacro(WorkingDirectory); 
	vtkGetStringMacro(WorkingDirectory); 

	// Description:
	// Set/get gnuplot command with path 
	vtkSetStringMacro(GnuplotCommand); 
	vtkGetStringMacro(GnuplotCommand); 

	// Description:
	// Set/get flag whether to hide gnuplot window on Windows.
	vtkSetMacro(HideWindow, bool); 
	vtkGetMacro(HideWindow, bool); 
	vtkBooleanMacro(HideWindow, bool); 
	
	// Description:
	// Set/get execution timeout of command 
	vtkSetMacro(Timeout, double); 
	vtkGetMacro(Timeout, double); 

protected:
	vtkGnuplotExecuter();
	virtual ~vtkGnuplotExecuter(); 

	char* WorkingDirectory; 
	char* GnuplotCommand; 
	std::vector<std::string> GnuplotArguments;

	bool HideWindow; 
	double Timeout; 



private:
	vtkGnuplotExecuter(const vtkGnuplotExecuter&);  // Not implemented.
	void operator=(const vtkGnuplotExecuter&);  // Not implemented.
}; 

#endif