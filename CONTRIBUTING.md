# Contributing to Pikafish

Welcome to the Pikafish project! We are excited that you are interested in
contributing. This document outlines the guidelines and steps to follow when
making contributions to Pikafish.

## Table of Contents

- [Building Pikafish](#building-pikafish)
- [Making Contributions](#making-contributions)
  - [Reporting Issues](#reporting-issues)
  - [Submitting Pull Requests](#submitting-pull-requests)
- [Code Style](#code-style)
- [Community and Communication](#community-and-communication)
- [License](#license)

## Building Pikafish

In case you do not have a C++ compiler installed, you can follow the
instructions from our wiki.

- [Linux][linux-compiling-link]
- [Windows][windows-compiling-link]
- [macOS][macos-compiling-link]

## Making Contributions

### Reporting Issues

If you find a bug, please open an issue on the
[issue tracker][issue-tracker-link]. Be sure to include relevant information
like your operating system, build environment, and a detailed description of the
problem.

_Please note that Pikafish's development is not focused on adding new features.
Thus any issue regarding missing features will potentially be closed without
further discussion._

### Submitting Pull Requests

- Functional changes need to be tested on fishtest. See
  [Creating my First Test][creating-my-first-test] for more details.
  The accompanying pull request should include a link to the test results and
  the new bench.

- Non-functional changes (e.g. refactoring, code style, documentation) do not
  need to be tested on fishtest, unless they might impact performance.

- Provide a clear and concise description of the changes in the pull request
  description.

_First time contributors should add their name to [AUTHORS](../AUTHORS)._

_Pikafish's development is not focused on adding new features. Thus any pull
request introducing new features will potentially be closed without further
discussion._

## Code Style

Changes to Pikafish C++ code should respect our coding style defined by
[.clang-format](.clang-format). You can format your changes by running
`make format`. This requires clang-format version 17 to be installed on your system.

## Community and Communication

- Join the [Pikafish discord][discord-link] to discuss ideas, issues, and
  development.
- Participate in the [Pikafish GitHub discussions][discussions-link] for
  broader conversations.

## License

By contributing to Pikafish, you agree that your contributions will be licensed
under the GNU General Public License v3.0. See [Copying.txt][copying-link] for
more details.

Thank you for contributing to Pikafish and helping us make it even better!

[copying-link]: https://github.com/official-pikafish/Pikafish/blob/master/Copying.txt
[discord-link]: https://discord.com/invite/uSb3RXb7cY
[discussions-link]: https://github.com/official-pikafish/Pikafish/discussions/new
[creating-my-first-test]: https://github.com/glinscott/fishtest/wiki/Creating-my-first-test#create-your-test
[issue-tracker-link]: https://github.com/official-pikafish/Pikafish/issues
[linux-compiling-link]: https://github.com/official-pikafish/Pikafish/wiki/Compiling-from-source#linux
[windows-compiling-link]: https://github.com/official-pikafish/Pikafish/wiki/Compiling-from-source#windows
[macos-compiling-link]: https://github.com/official-pikafish/Pikafish/wiki/Compiling-from-source#macos
