
#ifndef __VTKPLUSSTARTDATACOLLECTIONCOMMAND_H
#define __VTKPLUSSTARTDATACOLLECTIONCOMMAND_H


#include <string>
#include <vector>

#include "vtkObject.h"

#include "vtkDataCollector.h"
#include "vtkPlusCommand.h"



/**
 * This command starts the vtkDataCollector on the server side.
 */
class
VTK_EXPORT 
vtkPlusStartDataCollectionCommand
: public vtkPlusCommand
{
public:
  
  static vtkPlusStartDataCollectionCommand *New();
  vtkTypeRevisionMacro( vtkPlusStartDataCollectionCommand, vtkObject );
  virtual void PrintSelf( ostream& os, vtkIndent indent );
  
  virtual bool CanExecute( std::string str );
  virtual bool Execute();
  virtual std::string GetStringRepresentation();
  
protected:
  
  vtkPlusStartDataCollectionCommand();
  virtual ~vtkPlusStartDataCollectionCommand();
  
  
private:
	
  vtkPlusStartDataCollectionCommand( const vtkPlusStartDataCollectionCommand& );
  void operator=( const vtkPlusStartDataCollectionCommand& );
  
};


#endif

