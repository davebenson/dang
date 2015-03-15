# Introduction #

dang is not a well-specified language at the moment, but we wanted to write down some guiding principles.


# Details #

Important goals:
  * **embeddable**
    * coroutines/continuations
  * **extensible**
    * new types
    * new functions
    * generalized functions
  * **expressive**
    * concise syntax
    * type inference
  * **fast**
    * eventual support for JIT
  * **safe**
    * currently no program can crash (but they can throw exceptions)
    * do we want an "unsafe" directive to allow the occasional pointer

Less important goals:
  * **simple**
  * **small**

Someday:
  * **good library**
  * **thread support**