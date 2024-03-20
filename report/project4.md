## Project 4 Report


### 1. Basic information
- Team #: 22
- Github Repo Link: https://github.com/sahil60d/cs222-winter24-sahil60d.git
- Student 1 UCI NetID: sjdesai
- Student 1 Name: Sahil Desai
- Student 2 UCI NetID (if applicable):
- Student 2 Name (if applicable):


### 2. Catalog information about Index
- Show your catalog information about an index (tables, columns).
  - In the function create catalog, I create a tables, column and indices file. After the files have been properly created. I add them each as a table into the Tables file and I add the columns for each of them into the columns file. Once that is complete the catalog has been successfully created. I make sure to delete from the appropriate tables when deleting/destroying. 


### 3. Filter
- Describe how your filter works (especially, how you check the condition.)
  - When filter is constructed, it stores all the necessary information internally. getNextTuple works by iterating through the entire given operator and uses a method from rbfm, readAttributeFromRecord, to read the attribute data. It uses the method compareAttrs to compare the attribute found from iterator's getNextTuple and the condition stored in the contructor. If the condition is meet between the 2 attributes, the attribute data is stored and returned in *data. 



### 4. Project
- Describe how your project works.
  - When project is constructed, all the necessary information is stored internally
  - It uses iterators getNextTuple to get the next tuple
  - I then use a helper function to create a null-byte descriptor for the tuple
  - I also use a helper function to read the attribute data from the tuple in order to re-order it and store in *data according to the schema provided to us in attrNames
  - The attributes get stored into data in the appropriate order



### 5. Block Nested Loop Join
- Describe how your block nested loop join works (especially, how you manage the given buffers.)
  - My block nested loop join works be creating one page to store tuples for the inner loop, S, and uses the rest of the pages on the outer loop, R. Ever time that I iterate throught the inner loop, I am able to check it against a multitude more tuples from the outer layer.
  - I store the pointers to the pages in a vector<void*> that is also used to destory the pages.
  - I created a load left helper function that will malloc pages and store their pointers in the vector. 
  - I iterate throught the entire TableScan, then reload the left tuple and continue until completed.



### 6. Index Nested Loop Join
- Describe how your index nested loop join works.
  - My index nested loop join works by iterating through the left iterator and doing a nested loop with the right iterator. If the condtion is meet, I have a helper function that appropriately concatinates the attributes
  



### 7. Grace Hash Join (If you have implemented this feature)
- Describe how your grace hash join works (especially, in-memory structure).



### 8. Aggregation
- Describe how your basic aggregation works.
  - My group based aggregation works by performing the operations on the entire data set.
  - In getNextTuple I created a switch that accesses different helper functions based on the provided operation
  - The helper function getMIN, getMAX, getCOUNT, getSUM, and getAVG perform the actual calculations and generate the result.
  - min and max work by iterating through the entire dataset and comparing/store the min or max value
  - Sum and avg but iterate through the entire dataset and keep a count of the values. Avg also maintains a counter that is used to calculate the average from the sum.
  - count just iterated through the entire dataset and maintains a count. 


- Describe how your group-based aggregation works. (If you have implemented this feature)



### 9. Implementation Detail
- Have you added your own module or source file (.cc or .h)?
  Clearly list the changes on files and CMakeLists.txt, if any.



- Other implementation details:



### 10. Member contribution (for team of two)
- Explain how you distribute the workload in team.



### 11. Other (optional)
- Freely use this section to tell us about things that are related to the project 4, but not related to the other sections (optional)
  - Unfortunately I had to spend the majority of the time given to work on this project fixing code is rbfm, rm, and ix to ensure that my b+ tree implementation properly works. This left me with very little time to actually work on this project and essentially no time to debug. I managed to get my b+ tree implementation to adequalty work and pass more test cases and pass and the rbfm/rm test cases. I know that my code won't actually run and produce results but I think the idea behind all my implementation is on the right track.



- Feedback on the project to help improve the project. (optional)