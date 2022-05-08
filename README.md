Linux kernel
============

There are several guides for kernel developers and users. These guides can
be rendered in a number of formats, like HTML and PDF. Please read
Documentation/admin-guide/README.rst first.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.  The formatted documentation can also be read online at:

    https://www.kernel.org/doc/html/latest/

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.
See Documentation/00-INDEX for a list of what is contained in each file.

## To build inline you should:

- Please always use upstreamed-common to have latest LTS and CAF upstreams, with a fully tested
  changes imported to kernel.
- QK-eleven branch is the main branch that receives the latest patches in a experimental stage
  not advised to be used as inline kernel.
- Force pushes may occur, you have been warned.
