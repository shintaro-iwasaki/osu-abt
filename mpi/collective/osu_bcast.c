#define BENCHMARK "OSU MPI%s Broadcast Latency Test"
/*
 * Copyright (C) 2002-2021 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */
#include <osu_util_mpi.h>

int main(int argc, char *argv[])
{
    int i = 0, rank, size;
    int numprocs;
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0;
    double latency = 0.0, t_start = 0.0, t_stop = 0.0;
    double timer = 0.0;
    char *buffer = NULL;
    int po_ret;
    options.bench = COLLECTIVE;
    options.subtype = LAT;

    set_header(HEADER);
    set_benchmark_name("osu_bcast");
    po_ret = process_options(argc, argv);

    if (PO_OKAY == po_ret && NONE != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &numprocs));

    switch (po_ret) {
        case PO_BAD_USAGE:
            print_bad_usage_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_FAILURE);
        case PO_HELP_MESSAGE:
            print_help_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_VERSION_MESSAGE:
            print_version_message(rank);
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_OKAY:
            break;
    }

    if (numprocs < 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_CHECK(MPI_Finalize());
        exit(EXIT_FAILURE);
    }

    if (options.max_message_size > options.max_mem_limit) {
        if (rank == 0) {
            fprintf(stderr,
                    "Warning! Increase the Max Memory Limit to be able to run "
                    "up to %ld bytes.\n"
                    "Continuing with max message size of %ld bytes\n",
                    options.max_message_size, options.max_mem_limit);
        }
        options.max_message_size = options.max_mem_limit;
    }

    if (allocate_memory_coll((void **)&buffer, options.max_message_size,
                             options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
    }
    set_buffer(buffer, options.accel, 1, options.max_message_size);

    print_preamble(rank);

    for (size = options.min_message_size; size <= options.max_message_size;
         size *= 2) {
        if (size > LARGE_MESSAGE_SIZE) {
            options.skip = options.skip_large;
            options.iterations = options.iterations_large;
        }

        timer = 0.0;
        for (i = 0; i < options.iterations + options.skip; i++) {
            t_start = MPI_Wtime();
            MPI_CHECK(MPI_Bcast(buffer, size, MPI_CHAR, 0, MPI_COMM_WORLD));
            t_stop = MPI_Wtime();

            if (i >= options.skip) {
                timer += t_stop - t_start;
            }
            MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        }

        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

        latency = (timer * 1e6) / options.iterations;

        MPI_CHECK(MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
                             MPI_COMM_WORLD));
        MPI_CHECK(MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
                             MPI_COMM_WORLD));
        MPI_CHECK(MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
                             MPI_COMM_WORLD));
        avg_time = avg_time / numprocs;

        print_stats(rank, size, avg_time, min_time, max_time);
    }

    free_buffer(buffer, options.accel);

    MPI_CHECK(MPI_Finalize());

    if (NONE != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}

/* vi: set sw=4 sts=4 tw=80: */
