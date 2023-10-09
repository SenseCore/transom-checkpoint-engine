# Contributing guidelines

## Before contributing

Welcome to checkpoint-engine! Before sending your pull requests, make sure that you read the whole guidelines. If you have any doubt on the contributing guide, please feel free to state it clearly in an issue.

## Contributing

### Contributor

We are very happy that you are considering implementing the engine! This repository is suitable to pytorch-based deep-learning framework. Being one of our contributors, you agree and confirm that your submitted work fulfils or mostly fulfils our styles and standards

__New implementation__ is welcome! For example, new solutions for a scenario, different representations for a data structure or algorithm designs with less complexity.

__Improving comments__ and __writing proper tests__ are also highly welcome.

### Contribution

We appreciate any contribution, from fixing a grammar mistake in a comment to implementing complex algorithms. Please read this section if you are contributing your work.

Your contribution will be tested by our CI/CD to save time and mental energy. After you have submitted your pull request, you should see the CI/CD tests start to run. If those tests fail, read through the output to understand the failure.

#### What is precious?

Saving and loading checkpoints in a large-scale training is expensive, both by time and space. And our goal is to reduce the end-to-end training time, so the key is

* minimize user-aware latency during save & load
* minimize additional memory usage

#### formatter

Use [black](https://github.com/psf/black) to automatically format your python code to match our coding style:

```bash
pip install black  # only required the first time
```

Assume you're using VSCode, install python plugin, configure the `Python › Formatting: Black Path` with black installed. You can see the installed path by `which black`. Then configure **save on format**, the plugin will format your code every time you save a file.

Use [cpplint](https://github.com/cpplint/cpplint) to lint your cpp code.

Assume you're using VSCode, configure the formatter to be **clang**. For detailed formatting rules, refer to [.clang-format](.clang-format).

#### Python Coding Style

We want your work to be readable by others; therefore, we encourage you to note the following:

- Please write in Python 3.8+. For instance: `print()` is a function in Python 3 so `print "Hello"` will *not* work but `print("Hello")` will.
- Please focus hard on the naming of functions, classes, and variables.  Help your reader by using __descriptive names__ that can help you to remove redundant comments.
  - Single letter variable names are *old school* so please avoid them unless their life only spans a few lines.
  - Expand acronyms because `gcd()` is hard to understand but `greatest_common_divisor()` is not.
  - Please follow the [Python Naming Conventions](https://pep8.org/#prescriptive-naming-conventions) so variable_names and function_names should be lower_case, CONSTANTS in UPPERCASE, ClassNames should be CamelCase, etc.

- We encourage the use of Python [f-strings](https://realpython.com/python-f-strings/#f-strings-a-new-and-improved-way-to-format-strings-in-python) where they make the code easier to read.

- Original code submission require docstrings or comments to describe your work.

- More on docstrings and comments:

  The following are considered to be bad and may be requested to be improved:

  ```python
  x = x + 2	# increased by 2
  ```

  This is too trivial. Comments are expected to be explanatory. For comments, you can write them above, on or below a line of code, as long as you are consistent within the same piece of code.

  We encourage you to put docstrings inside your functions but please pay attention to the indentation of docstrings. The following is a good example:

  ```python
  def sum_ab(a, b):
      """
      Return the sum of two integers a and b.
      """
      return a + b
  ```

- Write tests (especially [__doctests__](https://docs.python.org/3/library/doctest.html)) to illustrate and verify your work.  We highly encourage the use of _doctests on all functions_.

  ```python
  def sum_ab(a, b):
      """
      Return the sum of two integers a and b
      >>> sum_ab(2, 2)
      4
      >>> sum_ab(-2, 3)
      1
      >>> sum_ab(4.9, 5.1)
      10.0
      """
      return a + b
  ```

  These doctests will be run by pytest as part of our automated testing so please try to run your doctests locally and make sure that they are found and pass:

  ```bash
  python3 -m doctest -v my_submission.py
  ```

  The use of the Python builtin `input()` function is __not__ encouraged:

  ```python
  input('Enter your input:')
  # Or even worse...
  input = eval(input("Enter your input: "))
  ```

  However, if your code uses `input()` then we encourage you to gracefully deal with leading and trailing whitespace in user input by adding `.strip()` as in:

  ```python
  starting_value = int(input("Please enter a starting value: ").strip())
  ```

  The use of [Python type hints](https://docs.python.org/3/library/typing.html) is encouraged for function parameters and return values.  Our automated testing will run [mypy](http://mypy-lang.org) so run that locally before making your submission.

  ```python
  def sum_ab(a: int, b: int) -> int:
      return a + b
  ```

  Instructions on how to install mypy can be found [here](https://github.com/python/mypy). Please use the command `mypy --ignore-missing-imports .` to test all files or `mypy --ignore-missing-imports path/to/file.py` to test a specific file.

- [__List comprehensions and generators__](https://docs.python.org/3/tutorial/datastructures.html#list-comprehensions) are preferred over the use of `lambda`, `map`, `filter`, `reduce` but the important thing is to demonstrate the power of Python in code that is easy to read and maintain.

- Avoid importing external libraries for basic algorithms. Only use those libraries for complicated algorithms.
- If you need a third-party module that is not in the file __requirements.txt__, please add it to that file as part of your submission.

#### C++ Coding Style

We want your work to be readable by others; therefore, we encourage you to note the following:

- Please use at least c++-11.
- Please focus hard on the naming of functions, classes, and variables.  Help your reader by using __descriptive names__ that can help you to remove redundant comments.
  - File names should be in all lowercase, and they can include underscores (_).
  - The first letter of each word in `class`, `typedef`, `using`, `enum` names should be capitalized, and they should not include underscores.
  - Variables (including function parameters) and data members should all be in lowercase, with words separated by underscores. Class member variables should end with an underscore, but this is not required for struct.
  - Regular functions should use a mix of uppercase and lowercase letters, while getter and setter functions are required to match the variable names. In general, function names should follow the "CamelCase" or "PascalCase" convention, where the first letter of each word is capitalized, and there are no underscores. For words with initial abbreviations, it is preferable to treat them as one word and capitalize the first letter (e.g., write `StartRpc()` instead of `StartRPC()`).
  - include order: The first to be included are the direct header file, e.g. a.cpp should first include a.h. Then C standard library, C++ standard library, headers from other libraries, and headers from your own project. 



#### Other Requirements for Submissions

- Please avoid creating new directories if at all possible. Try to fit your work into the existing directory structure.
- If possible, follow the standard *within* the folder you are submitting to.
- If you have modified/added code work, make sure the code compiles before submitting.
- If you have modified/added documentation work, ensure your language is concise and contains no grammar errors.
