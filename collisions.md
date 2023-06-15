| A          |  B           | Result   |
|------------|--------------|----------|
| none       | none         |abort     |
| none       | wall         |abort     |
| none       | objective    |abort     |
| none       | crate        |abort     |
| none       | crate_ok     |abort     |
| none       | character    |abort     |
| wall       | none         |abort     |
| wall       | wall         |abort     |
| wall       | objective    |abort     |
| wall       | crate        |abort     |
| wall       | crate_ok     |abort     |
| wall       | character    |abort     |
| objective  | none         |abort     |
| objective  | wall         |abort     |
| objective  | crate        |abort     |
| objective  | crate_ok     |abort     |
| objective  | character    |abort     |
| crate      | none         |go_through|
| crate      | wall         |noop      |
| crate      | objective    |noop      |
| crate      | crate        |noop      |
| crate      | crate_ok     |noop      |
| crate      | character    |abort     |
| crate_ok   | none         |go_through|
| crate_ok   | wall         |noop      |
| crate_ok   | objective    |go_through|
| crate_ok   | crate        |noop      |
| crate_ok   | crate_ok     |noop      |
| crate_ok   | character    |abort     |
| character  | none         |go_through|
| character  | wall         |noop      |
| character  | objective    |go_through|
| character  | crate        |push      |
| character  | crate_ok     |push      |
| character  | character    |abort     |


(character, none, *) go_through
(character, wall, *) noop
(character, objective, *) go_through
(character, crate, none) push
(character, crate, wall) noop
(character, crate, objective) push
(character, crate, crate) noop
(character, crate, character) abort
(character, character, *) abort

