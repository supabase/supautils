supautils is OSS. PR and issues are welcome.


## Development

[Nix](https://nixos.org/download.html) is required to set up the environment.


### Testing
For testing the module locally, execute:

```bash
# might take a while in downloading all the dependencies
$ nix-shell

# test on pg 12
$ supautils-with-pg-12 make installcheck

# test on pg 13
$ supautils-with-pg-13 make installcheck

# test on pg 14
$ supautils-with-pg-14 make installcheck

# you can also test manually with
$ supautils-with-pg-12 psql -U rolecreator
```
