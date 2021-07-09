# Build the SQL library
find stdlib -name '*.sql' -exec cat {} \; > sql/supautils--0.1.0.sql
