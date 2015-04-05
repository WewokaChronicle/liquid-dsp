// 
// qqdetector_example.c
//
// This example demonstrates...
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include "liquid.h"

#define OUTPUT_FILENAME "qdetector_cccf_example.m"

// print usage/help message
void usage()
{
    printf("qdetector_cccf_example\n");
    printf("options:\n");
    printf("  h     : print usage/help\n");
    printf("  n     : number of sync symbols, default: 80\n");
    printf("  F     : carrier frequency offset, default: 0.02\n");
    printf("  T     : fractional sample offset dt in [-0.5, 0.5], default: 0\n");
    printf("  S     : SNR [dB], default: 20\n");
    printf("  t     : detection threshold, default: 0.3\n");
}

int main(int argc, char*argv[])
{
    //srand(time(NULL));

    // options
    unsigned int sequence_len =   80;   // number of sync samples
    unsigned int k            =    2;
    unsigned int m            =    7;
    float        beta         =  0.3f;
    int          ftype        = LIQUID_FIRFILT_ARKAISER;
    float        gamma        =  1.0f;  // channel gain

#if 0
    float        noise_floor  = -30.0f; // noise floor [dB]
    float        SNRdB        =  20.0f; // signal-to-noise ratio [dB]
    float        threshold    =  0.3f;  // detection threshold
#endif
    float        tau          = -0.3f;  // fractional sample timing offset
    float        dphi         = -0.03f; // carrier frequency offset
    float        phi          =  0.5f;  // carrier phase offset

#if 0
    int dopt;
    while ((dopt = getopt(argc,argv,"hn:T:F:S:t:")) != EOF) {
        switch (dopt) {
        case 'h': usage();              return 0;
        case 'n': n         = atoi(optarg); break;
        case 'F': dphi      = atof(optarg); break;
        case 'T': dt        = atof(optarg); break;
        case 'S': SNRdB     = atof(optarg); break;
        case 't': threshold = atof(optarg); break;
        default:
            exit(1);
        }
    }
#endif

    unsigned int i;

#if 0
    // validate input
    if (tau < -0.5f || tau > 0.5f) {
        fprintf(stderr,"error: %s, fractional sample offset must be in [-0.5,0.5]\n", argv[0]);
        exit(1);
    }
#endif

    // derived values
    unsigned int num_symbols = 8*sequence_len + 2*m;
    unsigned int num_samples = k * num_symbols;
    float        nstd        = 0.1f;

    // arrays
    float complex x[num_samples];   // transmitted signal
    float complex y[num_samples];   // received signal

    // generate synchronization sequence (QPSK symbols)
    float complex sequence[sequence_len];
    for (i=0; i<sequence_len; i++) {
        sequence[i] = (rand() % 2 ? 1.0f : -1.0f) +
                      (rand() % 2 ? 1.0f : -1.0f) * _Complex_I;
    }

    // generate transmitted signal
    firinterp_crcf interp = firinterp_crcf_create_rnyquist(ftype, k, m, beta, -tau);
    unsigned int n = 0;
    for (i=0; i<num_symbols; i++) {
        // original sequence, then random symbols
        float complex sym = i < sequence_len ? sequence[i] : sequence[rand()%sequence_len];

        // interpolate
        firinterp_crcf_execute(interp, sym, &x[n]);
        n += k;
    }
    firinterp_crcf_destroy(interp);

    // 
    for (i=0; i<num_samples; i++) {
        y[i] = x[i];

        // channel gain
        y[i] *= gamma;

        // carrier offset
        y[i] *= cexpf(_Complex_I*(dphi*i + phi));
        
        // noise
        y[i] += nstd*(randnf() + _Complex_I*randnf())*M_SQRT1_2;
    }

    //
    float tau_hat   = 0.0f;
    float gamma_hat = 0.0f;
    float dphi_hat  = 0.0f;
    float phi_hat   = 0.0f;
    int   frame_detected = 0;

    // create detector
    qdetector_cccf q = qdetector_cccf_create(sequence, sequence_len, ftype, k, m, beta);
    qdetector_cccf_print(q);

    // delay
    unsigned int num_delay = 250;
    for (i=0; i<num_delay; i++)
        qdetector_cccf_execute(q,0.0f);

    //
    for (i=0; i<num_samples; i++) {
        float complex * v = qdetector_cccf_execute(q,y[i]);

        if (v != NULL) {
            printf("\nframe detected!\n");
            frame_detected = 1;

            // get statistics
            tau_hat   = qdetector_cccf_get_tau    (q);
            gamma_hat = qdetector_cccf_get_gamma  (q);
            dphi_hat  = qdetector_cccf_get_dphi   (q);
            phi_hat   = qdetector_cccf_get_phi    (q);
            break;

            // TODO: apply matched filter, etc. and recover symbols
        }
    }

    // destroy objects
    qdetector_cccf_destroy(q);

    // print results
    printf("\n");
    printf("frame detected  :   %s\n", frame_detected ? "yes" : "no");
    printf("  gamma hat     : %8.3f, actual=%8.3f (error=%8.3f)\n",            gamma_hat, gamma, gamma_hat - gamma);
    printf("  tau hat       : %8.3f, actual=%8.3f (error=%8.3f) samples\n",    tau_hat,   tau,   tau_hat   - tau  );
    printf("  dphi hat      : %8.5f, actual=%8.5f (error=%8.5f) rad/sample\n", dphi_hat,  dphi,  dphi_hat  - dphi );
    printf("  phi hat       : %8.5f, actual=%8.5f (error=%8.5f) radians\n",    phi_hat,   phi,   phi_hat   - phi  );
    printf("\n");

    // 
    // export results
    //
    FILE * fid = fopen(OUTPUT_FILENAME,"w");
    fprintf(fid,"%% %s : auto-generated file\n", OUTPUT_FILENAME);
    fprintf(fid,"clear all\n");
    fprintf(fid,"close all\n");
    fprintf(fid,"sequence_len= %u;\n", sequence_len);
    fprintf(fid,"num_samples = %u;\n", num_samples);

    fprintf(fid,"x = zeros(1,num_samples);\n");
    fprintf(fid,"y = zeros(1,num_samples);\n");
    for (i=0; i<num_samples; i++) {
        fprintf(fid,"x(%4u) = %12.8f + j*%12.8f;\n", i+1, crealf(x[i]), cimagf(x[i]));
        fprintf(fid,"y(%4u) = %12.8f + j*%12.8f;\n", i+1, crealf(y[i]), cimagf(y[i]));
    }

    fprintf(fid,"t=[0:(num_samples-1)];\n");
    fprintf(fid,"figure;\n");
    fprintf(fid,"subplot(2,1,1);\n");
    fprintf(fid,"  plot(t,real(x), t,imag(x));\n");
    fprintf(fid,"  grid on;\n");
    fprintf(fid,"  xlabel('time');\n");
    fprintf(fid,"  ylabel('transmitted signal');\n");
    fprintf(fid,"subplot(2,1,2);\n");
    fprintf(fid,"  plot(t,real(y), t,imag(y));\n");
    fprintf(fid,"  grid on;\n");
    fprintf(fid,"  xlabel('time');\n");
    fprintf(fid,"  ylabel('received signal');\n");

    fclose(fid);
    printf("results written to '%s'\n", OUTPUT_FILENAME);

    return 0;
}
