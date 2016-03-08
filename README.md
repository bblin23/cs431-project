# cs431-project
  
----  
  
After pulling, how do you compile and run somebody else's code?  
  
	cd $OS161TOP/os161-1.99/    
	// Edit configure using sublime-text or gedit or whatever. ~line 30 where it says OSTREE=  
	// Set OSTREE to equal where your OS161 root is.  
	// Example: OSTREE='$(HOME)/mygitrepo/cs431-os161/root'  
	./configure --ostree=$HOME/mygitrepo/cs431-os161/root  
	cd kern/conf  
	./config ASST#  
	cd ../compile/ASST#  
	bmake clean  
	bmake depend  
	bmake  
	bmake install  
