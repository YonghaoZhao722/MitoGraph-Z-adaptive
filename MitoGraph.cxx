// ==================================================================
// MitoGraph: Quantifying Mitochondrial Content in Living Cells
// Written by Matheus P. Viana - vianamp@gmail.com - 2018.01.08
// Last update: 2024.06.20
// 
// Susanne Rafelski Lab, University of California Irvine
//
// The official documentation can be found at
//
//      The official software website
//      - https://github.com/vianamp/MitoGraph
//
// A protocol paper describing how to use MitoGraph is available in:
// Quantifying mitochondrial content in living cells
// http://www.sciencedirect.com/science/article/pii/S0091679X14000041
// ==================================================================

#include "ssThinning.h"
#include "MitoThinning.h"

    double _rad = 0.150;
    double _dxy, _dz = -1.0;
    bool _export_graph_files = true;
    bool _export_image_binary = false;
    bool _export_image_resampled = false;
    bool _scale_polydata_before_save = true;
    bool _export_nodes_label = true;
    double _div_threshold = 0.1666667;
    bool _checkonly = false;
    double _resample = -1.0;
    bool _improve_skeleton_quality = true; // when this is true nodes with degree zero
                                           // expanded and detected. Additional checking
                                           // is also done to garantee that all non-zero
                                           // voxels were analysized.

    std::string MITOGRAPH_VERSION = "v3.1";

    //                    |------06------|
    //                    |------------------------18------------------------|
    //                    |---------------------------------------26----------------------------------|
    int ssdx_sort[26] = { 0,-1, 0, 1, 0, 0,-1, 0, 1, 0,-1, 1, 1,-1,-1, 0, 1, 0, -1, 1, 1,-1,-1, 1, 1,-1};
    int ssdy_sort[26] = { 0, 0,-1, 0, 1, 0, 0,-1, 0, 1,-1,-1, 1, 1, 0,-1, 0, 1, -1,-1, 1, 1,-1,-1, 1, 1};
    int ssdz_sort[26] = {-1, 0, 0, 0, 0, 1,-1,-1,-1,-1, 0, 0, 0, 0, 1, 1, 1, 1, -1,-1,-1,-1, 1, 1, 1, 1};

/**========================================================
 Auxiliar functions
 =========================================================*/


// This routine returns the x of the id-th point of a 3D volume
// of size Dim[0]xDim[1]xDim[2]
int  GetX(vtkIdType id, int *Dim);

// This routine returns the y of the id-th point of a 3D volume
// of size Dim[0]xDim[1]xDim[2]
int  GetY(vtkIdType id, int *Dim);

// This routine returns the z of the id-th point of a 3D volume
// of size Dim[0]xDim[1]xDim[2]
int  GetZ(vtkIdType id, int *Dim);

// This routine returns the id of a point located at coordinate
// (x,y,z) of a 3D volume of size Dim[0]xDim[1]xDim[2]
vtkIdType GetId(int x, int y, int z, int *Dim);

// Swap values
void Swap(double *x, double *y);

// Simple sorting algorithm and the output is such that
// l3 >= l2 >= l3
void Sort(double *l1, double *l2, double *l3);

// Calculate the Frobenius norm of a given 3x3 matrix
// http://mathworld.wolfram.com/FrobeniusNorm.html
double FrobeniusNorm(double M[3][3]);

// This routine scales the polydata points to the correct dimension
// given by parameters _dxy and _dz.
void ScalePolyData(vtkSmartPointer<vtkPolyData> PolyData, _mitoObject *mitoObject);

// Stores all files with a given extension in a vector
int ScanFolderForThisExtension(std::string _impath, std::string ext, std::vector<std::string> List);

/* ================================================================
   IMAGE TRANSFORM
=================================================================*/

// This routine converts 16-bit volumes into 8-bit volumes by
// linearly scaling the original range of intensities [min,max]
// in [0,255] (http://rsbweb.nih.gov/ij/docs/guide/146-28.html)
vtkSmartPointer<vtkImageData> Convert16To8bit(vtkSmartPointer<vtkImageData> Image);

// Z-adaptive version that normalizes each z-plane independently
vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptive(vtkSmartPointer<vtkImageData> Image);

// Z-block adaptive version that normalizes z-planes in blocks
vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptiveBlocks(vtkSmartPointer<vtkImageData> Image, int block_size);

// Gentle z-adaptive version that preserves 3D continuity while enhancing dimmer areas
vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptiveGentle(vtkSmartPointer<vtkImageData> Image, int block_size);

// Apply a threshold to a ImageData and converts the result in
// a 8-bit ImageData.
vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToChar(vtkSmartPointer<vtkImageData> Image, double threshold);

// Z-adaptive version that calculates different thresholds for each z-plane
vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToCharZAdaptive(vtkSmartPointer<vtkImageData> Image, double base_threshold);

// Conservative z-adaptive version that uses blocks to reduce noise
vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToCharZAdaptiveConservative(vtkSmartPointer<vtkImageData> Image, double base_threshold, int z_block_size);

// Enhance structural connectivity before binarization
vtkSmartPointer<vtkImageData> EnhanceStructuralConnectivity(vtkSmartPointer<vtkImageData> Image, double sigma);

// Connect fragmented skeleton segments
vtkSmartPointer<vtkPolyData> ConnectSkeletonFragments(vtkSmartPointer<vtkPolyData> Skeleton, double max_gap_distance);

// Fill holes in the 3D image
void FillHoles(vtkSmartPointer<vtkImageData> ImageData);

// This routine uses a nearest neighbour filter to deblur the
// 16bit original image before applying MitoGraph.
// [1] - Qiang Wu Fatima A. Merchant Kenneth R. Castleman
//       Microscope Image Processing (pg.340)
// ????

/* ================================================================
   I/O ROUTINES
=================================================================*/

// Export maximum projection of a given ImageData as a
// PNG file.
void ExportMaxProjection(vtkSmartPointer<vtkImageData> Image, const char FileName[], bool binary);

// Export maximum projection of bottom and top parts of
// a given Tiff image as a PNG file as well as the polydata
// surface points
void ExportDetailedMaxProjection(_mitoObject *mitoObject);

// Export results in global as well as individual files
void DumpResults(_mitoObject mitoObject);

// Export configuration file used to run MitoGraph
void ExportConfigFile(_mitoObject *mitoObject);

/* ================================================================
   ROUTINES FOR VESSELNESS CALCUATION VIA DISCRETE APPROCH
=================================================================*/

// This routine uses a discrete differential operator to
// calculate the derivatives of a given 3D volume
void GetImageDerivativeDiscrete(vtkSmartPointer<vtkDataArray> Image, int *dim, char direction, vtkSmartPointer<vtkFloatArray> Derivative);

// This routine calculate the Hessian matrix for each point
// of a 3D volume and its eigenvalues (Discrete Approach)
void GetHessianEigenvaluesDiscrete(double sigma, vtkSmartPointer<vtkImageData> Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3);
void GetHessianEigenvaluesDiscreteZDependentThreshold(double sigma, vtkImageData *Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3, _mitoObject *mitoObjectt);

// Calculate the vesselness at each point of a 3D volume based
// based on the Hessian eigenvalues
void GetVesselness(double sigma, vtkSmartPointer<vtkImageData> Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3, _mitoObject *mitoObjectt);

// Calculate the vesselness over a range of different scales
int MultiscaleVesselness(_mitoObject *mitoObject);

/* ================================================================
   DIVERGENCE FILTER
=================================================================*/

// This routine calculates the divergence filter of a 3D volume
// based on the orientation of the gradient vector field
void GetDivergenceFilter(int *Dim, vtkSmartPointer<vtkDoubleArray> Scalars);

/* ================================================================
   WIDTH ANALYSIS
=================================================================*/

// Approximation of tubule width by the distance of the skeleton
// to the closest point over the surface.
void EstimateTubuleWidth(vtkSmartPointer<vtkPolyData> Skeleton, vtkSmartPointer<vtkPolyData> Surface, _mitoObject *mitoObject);

/* ================================================================
   INTENSITY MAPPING
=================================================================*/

// Intensities of the original TIFF image is mapped into a scalar
// component of the skeleton.
void MapImageIntensity(vtkSmartPointer<vtkPolyData> Skeleton, vtkSmartPointer<vtkImageData> ImageData, int nneigh);

/**========================================================
 Auxiliar functions
 =========================================================*/

int GetX(vtkIdType id, int *Dim) {
    return (int) ( id % (vtkIdType)Dim[0] );
}

int GetY(vtkIdType id, int *Dim) {
    return (int) (  ( id % (vtkIdType)(Dim[0]*Dim[1]) ) / (vtkIdType)Dim[0]  );
}

int GetZ(vtkIdType id, int *Dim) {
    return (int) ( id / (vtkIdType)(Dim[0]*Dim[1]) );
}

vtkIdType GetId(int x, int y, int z, int *Dim) {
    return (vtkIdType)(x+y*Dim[0]+z*Dim[0]*Dim[1]);
}

vtkIdType GetReflectedId(int x, int y, int z, int *Dim) {
    int rx = ceil(0.5*Dim[0]);
    int ry = ceil(0.5*Dim[1]);
    int rz = ceil(0.5*Dim[2]);
    int sx = (x-(rx-0.5)<0) ? -rx : Dim[0]-rx;
    int sy = (y-(ry-0.5)<0) ? -ry : Dim[1]-ry;
    int sz = (z-(rz-0.5)<0) ? -rz : Dim[2]-rz;
    return GetId(x-sx,y-sy,z-sz,Dim);
}

void Swap(double *x, double *y) {
    double t = *y; *y = *x; *x = t;
}

void Sort(double *l1, double *l2, double *l3) {
    if (fabs(*l1) > fabs(*l2)) Swap(l1,l2);
    if (fabs(*l2) > fabs(*l3)) Swap(l2,l3);
    if (fabs(*l1) > fabs(*l2)) Swap(l1,l2);
}

double FrobeniusNorm(double M[3][3]) {
    double f = 0.0;
    for (int i = 3;i--;)
        for (int j = 3;j--;)
            f += M[i][j]*M[i][j];
    return sqrt(f);
}

void ScalePolyData(vtkSmartPointer<vtkPolyData> PolyData, _mitoObject *mitoObject) {
    double r[3];
    vtkPoints *Points = PolyData -> GetPoints();
    for (vtkIdType id = 0; id < Points -> GetNumberOfPoints(); id++) {
        Points -> GetPoint(id,r);
        Points -> SetPoint(id,_dxy*(r[0]+mitoObject->Ox),_dxy*(r[1]+mitoObject->Oy),_dz*(r[2]+mitoObject->Oz));
    }
    Points -> Modified();
}

double SampleBackgroundIntensity(vtkDataArray *Scalar) {
    int N = 1000;
    double v = 1E6;
    vtkIdType i, j;
    for (i = N; i--;) {
        j = (vtkIdType) (rand() % (Scalar->GetNumberOfTuples()));
        v = (Scalar->GetTuple1(j)<v) ? Scalar->GetTuple1(j) : v;
    }
    return v;
}

int PoissonGen(double mu) {
    int k = 0;
    double p = 1.0, L = exp(-mu);
    do {
        k++;
        p *= (double)(rand()) / RAND_MAX;
    } while (p>L);
    return k;
}

int ScanFolderForThisExtension(std::string _root, std::string ext, std::vector<std::string> *List) {
    DIR *dir;
    int ext_p;
    struct dirent *ent;
    std::string _dir_name;
    if ((dir = opendir (_root.c_str())) != NULL) {
      while ((ent = readdir (dir)) != NULL) {
        _dir_name = std::string(ent->d_name);
        ext_p = (int)_dir_name.find(ext);
        if (ext_p > 0) {
            #ifdef DEBUG
                // Debug output removed
            #endif
            List -> push_back(std::string(_root)+_dir_name.substr(0,ext_p));
        }
      }
      closedir (dir);
    } else {
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* ================================================================
   I/O ROUTINES
=================================================================*/

void ExportMaxProjection(vtkSmartPointer<vtkImageData> Image, const char FileName[]) {

    #ifdef DEBUG
        // Debug output removed
    #endif

    int *Dim = Image -> GetDimensions();
    vtkSmartPointer<vtkImageData> MaxP = vtkSmartPointer<vtkImageData>::New();
    MaxP -> SetDimensions(Dim[0],Dim[1],1);
    vtkIdType N = Dim[0] * Dim[1];

    vtkSmartPointer<vtkUnsignedCharArray> MaxPArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
    MaxPArray -> SetNumberOfComponents(1);
    MaxPArray -> SetNumberOfTuples(N);

    int x, y, z;
    double v, vproj;
    for (x = Dim[0]; x--;) {
        for (y = Dim[1]; y--;) {
            vproj = 0;
            for (z = Dim[2]; z--;) {
                v = Image -> GetScalarComponentAsFloat(x,y,z,0);
                vproj = (v > vproj) ? v : vproj;
            }
            double point[3] = {(double)x, (double)y, 0.0};
            MaxPArray -> SetTuple1(MaxP->FindPoint(point),(unsigned char)vproj);
        }
    }
    MaxPArray -> Modified();

    MaxP -> GetPointData() -> SetScalars(MaxPArray);

    vtkSmartPointer<vtkPNGWriter> PNGWriter = vtkSmartPointer<vtkPNGWriter>::New();
    PNGWriter -> SetFileName(FileName);
    PNGWriter -> SetFileDimensionality(2);
    PNGWriter -> SetCompressionLevel(0);
    PNGWriter -> SetInputData(MaxP);
    PNGWriter -> Write();

    #ifdef DEBUG
        // Debug output removed
    #endif

}

void ExportDetailedMaxProjection(_mitoObject *mitoObject) {

    // =======================================================================================
    //                 |                |                 |                |                 |
    //  Total MaxProj  |      First     |       Top       |       Top      |       Top       |
    // (original tiff) |      Slice     |     8 slices    |     surface    |     skeleton    |
    //                 |                |                 |                |                 |
    // =======================================================================================
    //                 |                |                 |                |                 |
    //  Total MaxProj  |      Last      |      Bottom     |     Bottom     |     Bottom      |
    //    (surface)    |      Slice     |     8 slices    |     surface    |     skeleton    |
    //                 |                |                 |                |                 |
    // =======================================================================================

    #ifdef DEBUG
        printf("Saving Detailed Max projection...\n");
    #endif

    vtkSmartPointer<vtkTIFFReader> TIFFReader = vtkSmartPointer<vtkTIFFReader>::New();
    int errlog = TIFFReader -> CanReadFile((mitoObject->FileName+".tif").c_str());
    // File cannot be opened
    if (!errlog) {

        printf("File %s connot be opened.\n",(mitoObject->FileName+".tif").c_str());

    } else {

        // Loading TIFF File
        TIFFReader -> SetFileName((mitoObject->FileName+".tif").c_str());
        TIFFReader -> Update();
        vtkSmartPointer<vtkImageData> Image = TIFFReader -> GetOutput();

        //16bit to 8bit Conversion
        Image = Convert16To8bit(Image);

        // Loading PolyData Surface
        vtkSmartPointer<vtkPolyDataReader> PolyDaTaReader = vtkSmartPointer<vtkPolyDataReader>::New();
        PolyDaTaReader -> SetFileName((mitoObject->FileName+"_mitosurface.vtk").c_str());
        PolyDaTaReader -> Update();
        vtkSmartPointer<vtkPolyData> Surface = PolyDaTaReader -> GetOutput();

        // Loading Skeleton
        vtkSmartPointer<vtkPolyDataReader> PolyDaTaReaderSkell = vtkSmartPointer<vtkPolyDataReader>::New();
        PolyDaTaReaderSkell -> SetFileName((mitoObject->FileName+"_skeleton.vtk").c_str());
        PolyDaTaReaderSkell -> Update();
        vtkSmartPointer<vtkPolyData> Skeleton = PolyDaTaReaderSkell -> GetOutput();

        #ifdef DEBUG
            printf("\t#Points [%s] = %d\n",(mitoObject->FileName+"_mitosurface.vtk").c_str(),(int)Surface->GetNumberOfPoints());
            printf("\t#Points [%s] = %d\n",(mitoObject->FileName+"_skeleton.vtk").c_str(),(int)Skeleton->GetNumberOfPoints());
        #endif

        // Stack Dimensions
        int *Dim = Image -> GetDimensions();
        
        #ifdef DEBUG
            // Debug output removed
        #endif

        // Surface Bounds
        double *Bounds = Surface -> GetBounds();

        int zi = round(Bounds[4]/_dz); zi -= (zi>1) ? 1 : 0;
        int zf = round(Bounds[5]/_dz); zf += (zi<Dim[2]-1) ? 1 : 0;

        #ifdef DEBUG
            // Debug output removed
        #endif

        // Plane
        vtkSmartPointer<vtkImageData> Plane = vtkSmartPointer<vtkImageData>::New();
        Plane -> SetDimensions(5*Dim[0],2*Dim[1],1);
        vtkIdType N = 10 * Dim[0] * Dim[1];

        // Scalar VEctor
        vtkSmartPointer<vtkUnsignedCharArray> MaxPArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
        MaxPArray -> SetNumberOfComponents(1);
        MaxPArray -> SetNumberOfTuples(N);
        double range[2];
        Image -> GetScalarRange(range);
        MaxPArray -> FillComponent(0,range[0]);

        #ifdef DEBUG
            // Debug output removed
        #endif

        //Max Projection bottom
        int x, y, z;
        double v, vproj;
        for (x = Dim[0]; x--;) {
            for (y = Dim[1]; y--;) {
                vproj = 0;
                for (z = zi; z <= zi+8; z++) {
                    v = Image -> GetScalarComponentAsFloat(x,y,z,0);
                    vproj = (v > vproj) ? v : vproj;
                }
                double point[3] = {(double)(x+2*Dim[0]), (double)y, 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point),(unsigned char)vproj);
            }
        }
        //Max Projection top
        for (x = Dim[0]; x--;) {
            for (y = Dim[1]; y--;) {
                vproj = 0;
                for (z = zf-8; z <= zf; z++) {
                    v = Image -> GetScalarComponentAsFloat(x,y,z,0);
                    vproj = (v > vproj) ? v : vproj;
                }
                double point[3] = {(double)(x+2*Dim[0]), (double)(y+Dim[1]), 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point),(unsigned char)vproj);
            }
        }

        // Partial surface Projection
        double r[3];
        for (vtkIdType id=0; id < Surface -> GetPoints() -> GetNumberOfPoints(); id++) {
            Surface -> GetPoints() -> GetPoint(id,r);
            x = round(r[0]/_dxy);
            y = round(r[1]/_dxy);
            z = round(r[2]/_dz);
            if ( z >= zi && z <= zi+8 ) {
                double point[3] = {(double)(x+3*Dim[0]), (double)y, 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point),255);
            }
            if ( z >= zf-8 && z <= zf ) {
                double point2[3] = {(double)(x+3*Dim[0]), (double)(y+Dim[1]), 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point2),255);
            }
        }

        // Complete surface Projection
        for (vtkIdType id=0; id < Surface -> GetPoints() -> GetNumberOfPoints(); id++) {
            Surface -> GetPoints() -> GetPoint(id,r);
            x = round(r[0]/_dxy);
            y = round(r[1]/_dxy);
            z = round(r[2]/_dz);
            double point[3] = {(double)x, (double)y, 0.0};
            MaxPArray -> SetTuple1(Plane->FindPoint(point),255);
        }

        // Partial skeleton Projection
        for (vtkIdType id=0; id < Skeleton -> GetPoints() -> GetNumberOfPoints(); id++) {
            Skeleton -> GetPoints() -> GetPoint(id,r);
            x = round(r[0]/_dxy);
            y = round(r[1]/_dxy);
            z = round(r[2]/_dz);
            if ( z >= zi && z <= zi+8 ) {
                double point[3] = {(double)(x+4*Dim[0]), (double)y, 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point),255);
            }
            if ( z >= zf-8 && z <= zf ) {
                double point2[3] = {(double)(x+4*Dim[0]), (double)(y+Dim[1]), 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point2),255);
            }
        }

        //Complete max projection
        for (x = Dim[0]; x--;) {
            for (y = Dim[1]; y--;) {
                vproj = 0;
                for (z = 0; z < Dim[2]; z++) {
                    v = Image -> GetScalarComponentAsFloat(x,y,z,0);
                    vproj = (v > vproj) ? v : vproj;
                }
                double point[3] = {(double)x, (double)(y+Dim[1]), 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point),(unsigned char)vproj);
            }
        }

        //First and last slice
        for (x = Dim[0]; x--;) {
            for (y = Dim[1]; y--;) {
                v = Image -> GetScalarComponentAsFloat(x,y,0,0);
                double point1[3] = {(double)(x+Dim[1]), (double)y, 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point1),(unsigned char)v);
                v = Image -> GetScalarComponentAsFloat(x,y,Dim[2]-1,0);
                double point2[3] = {(double)(x+Dim[1]), (double)(y+Dim[1]), 0.0};
                MaxPArray -> SetTuple1(Plane->FindPoint(point2),(unsigned char)v);
            }
        }

        MaxPArray -> Modified();

        Plane -> GetPointData() -> SetScalars(MaxPArray);

        // Saving PNG File
        vtkSmartPointer<vtkPNGWriter> PNGWriter = vtkSmartPointer<vtkPNGWriter>::New();
        PNGWriter -> SetFileName((mitoObject->FileName+"_detailed.png").c_str());
        PNGWriter -> SetFileDimensionality(2);
        PNGWriter -> SetCompressionLevel(0);
        PNGWriter -> SetInputData(Plane);
        PNGWriter -> Write();

        // Debug output removed

    }

    #ifdef DEBUG
        // Debug output removed
    #endif

}

void DumpResults(const _mitoObject mitoObject) {

    // Saving network attributes in the individual file
    FILE *findv = fopen((mitoObject.FileName+".mitograph").c_str(),"w");

    for (int i = 0; i < mitoObject.attributes.size(); i++)
        fprintf(findv,"%s\t",mitoObject.attributes[i].name.c_str());
    fprintf(findv,"\n");
    for (int i = 0; i < mitoObject.attributes.size(); i++)
        fprintf(findv,"%1.5f\t",mitoObject.attributes[i].value);
    fprintf(findv,"\n");
    fclose(findv);

    // Also printing on the screen
    // Debug output removed

}

void ExportConfigFile(const _mitoObject mitoObject) {

    const char *_t = "[True]";
    const char *_f = "[False]";

    FILE *f = fopen((mitoObject.Folder+"/mitograph.config").c_str(),"w");

    if (mitoObject._adaptive_threshold) {
        fprintf(f,"MitoGraph %s [Adaptive Algorithm]\n",MITOGRAPH_VERSION.c_str());
    } else {
        fprintf(f,"MitoGraph %s\n",MITOGRAPH_VERSION.c_str());
    }
    fprintf(f,"Folder: %s\n",mitoObject.Folder.c_str());
    if (mitoObject._adaptive_threshold) {
        fprintf(f,"NBlocks: %d\n",mitoObject._nblks);
    }
    if (mitoObject._z_adaptive) {
        fprintf(f,"Z-Adaptive Processing: %s\n",_t);
        fprintf(f,"Z-Block Size: %d\n",mitoObject._z_block_size);
    }
    fprintf(f,"Enhance Connectivity: %s\n",mitoObject._enhance_connectivity?_t:_f);
    if (mitoObject._smart_component_filtering) {
        fprintf(f,"Smart Component Filtering: %s\n",_t);
        fprintf(f,"Min Component Size: %d\n",mitoObject._min_component_size);
    } else {
        fprintf(f,"Smart Component Filtering: %s\n",_f);
    }
    fprintf(f,"Pixel size: -xy %1.4fum, -z %1.4fum\n",_dxy,_dz);
    fprintf(f,"Average tubule radius: -r %1.4fum\n",_rad);
    fprintf(f,"Scales: -scales %1.2f",mitoObject._sigmai);
    for ( double sigma = mitoObject._sigmai+mitoObject._dsigma; sigma < mitoObject._sigmaf+0.5*mitoObject._dsigma; sigma += mitoObject._dsigma )
        fprintf(f," %1.2f",sigma);
    fprintf(f,"\nPost-divergence threshold: -threshold %1.5f\n",_div_threshold);
    fprintf(f,"Input type: %s\n",mitoObject.Type.c_str());
    fprintf(f,"Analyze: %s\n",mitoObject._analyze?_t:_f);
    fprintf(f,"Binary input: %s\n",mitoObject._binary_input?_t:_f);
    fprintf(f,"Z-Adaptive: %s\n",mitoObject._z_adaptive?_t:_f);
    time_t now = time(0);
    fprintf(f,"%s\n",ctime(&now));
    fclose(f);

}

/* ================================================================
   IMAGE TRANSFORM
=================================================================*/

vtkSmartPointer<vtkImageData> Convert16To8bit(vtkSmartPointer<vtkImageData> Image) {

    // 8-Bit images
    if (Image -> GetScalarType() == VTK_UNSIGNED_CHAR) {

        return Image;

    // 16-Bit images
    } else if (Image -> GetScalarType() == VTK_UNSIGNED_SHORT) {

        #ifdef DEBUG
            // Debug output removed
        #endif

        vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
        Image8 -> ShallowCopy(Image);

        vtkDataArray *ScalarsShort = Image -> GetPointData() -> GetScalars();
        unsigned long int N = ScalarsShort -> GetNumberOfTuples();
        double range[2];
        ScalarsShort -> GetRange(range);

        #ifdef DEBUG
            // Debug output removed
        #endif

        vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
        ScalarsChar -> SetNumberOfComponents(1);
        ScalarsChar -> SetNumberOfTuples(N);
        
        double x, y;
        vtkIdType id;
        for ( id = N; id--; ) {
            x = ScalarsShort -> GetTuple1(id);
            y = 255.0 * (x-range[0]) / (range[1]-range[0]);
            ScalarsChar -> SetTuple1(id,(unsigned char)y);
        }
        ScalarsChar -> Modified();

        Image8 -> GetPointData() -> SetScalars(ScalarsChar);
        return Image8;

    // Other depth
    } else {
        return NULL;
    }
}

vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptive(vtkSmartPointer<vtkImageData> Image) {

    // 8-Bit images
    if (Image -> GetScalarType() == VTK_UNSIGNED_CHAR) {

        return Image;

    // 16-Bit images
    } else if (Image -> GetScalarType() == VTK_UNSIGNED_SHORT) {

            // Debug output removed

        int *Dim = Image -> GetDimensions();
        vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
        Image8 -> ShallowCopy(Image);

        vtkDataArray *ScalarsShort = Image -> GetPointData() -> GetScalars();
        unsigned long int N = ScalarsShort -> GetNumberOfTuples();

        vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
        ScalarsChar -> SetNumberOfComponents(1);
        ScalarsChar -> SetNumberOfTuples(N);
        
        // Process each z-plane independently
        for (int z = 0; z < Dim[2]; z++) {
            
            // Find min/max for this z-plane
            double z_min = DBL_MAX, z_max = DBL_MIN;
            for (int x = 0; x < Dim[0]; x++) {
                for (int y = 0; y < Dim[1]; y++) {
                    double val = Image -> GetScalarComponentAsDouble(x, y, z, 0);
                    if (val < z_min) z_min = val;
                    if (val > z_max) z_max = val;
                }
            }
            
            // Debug output removed
            
            // Normalize this z-plane
            double z_range = z_max - z_min;
            if (z_range > 0) {
                for (int x = 0; x < Dim[0]; x++) {
                    for (int y = 0; y < Dim[1]; y++) {
                        double point[3] = {(double)x, (double)y, (double)z};
                        vtkIdType id = Image -> FindPoint(point);
                        double val = ScalarsShort -> GetTuple1(id);
                        double normalized = 255.0 * (val - z_min) / z_range;
                        ScalarsChar -> SetTuple1(id, (unsigned char)normalized);
                    }
                }
            } else {
                // If z-plane has uniform intensity, set to middle gray
                for (int x = 0; x < Dim[0]; x++) {
                    for (int y = 0; y < Dim[1]; y++) {
                        double point[3] = {(double)x, (double)y, (double)z};
                        vtkIdType id = Image -> FindPoint(point);
                        ScalarsChar -> SetTuple1(id, 128);
                    }
                }
            }
        }
        
        ScalarsChar -> Modified();
        Image8 -> GetPointData() -> SetScalars(ScalarsChar);
        return Image8;

    // Other depth
    } else {
        return NULL;
    }
}

vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptiveBlocks(vtkSmartPointer<vtkImageData> Image, int block_size) {

    // 8-Bit images
    if (Image -> GetScalarType() == VTK_UNSIGNED_CHAR) {
        return Image;
    }

    // 16-Bit images
    if (Image -> GetScalarType() == VTK_UNSIGNED_SHORT) {

            // Debug output removed

        int *Dim = Image -> GetDimensions();
        vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
        Image8 -> ShallowCopy(Image);

        vtkDataArray *ScalarsShort = Image -> GetPointData() -> GetScalars();
        unsigned long int N = ScalarsShort -> GetNumberOfTuples();

        vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
        ScalarsChar -> SetNumberOfComponents(1);
        ScalarsChar -> SetNumberOfTuples(N);
        
        // Calculate global statistics for fallback
        double global_range[2];
        ScalarsShort -> GetRange(global_range);
        
        // Process z-planes in blocks
        for (int z_start = 0; z_start < Dim[2]; z_start += block_size) {
            
            int z_end = (z_start + block_size < Dim[2]) ? z_start + block_size : Dim[2];
            
            // Find min/max for this z-block
            double block_min = DBL_MAX, block_max = DBL_MIN;
            for (int z = z_start; z < z_end; z++) {
                for (int x = 0; x < Dim[0]; x++) {
                    for (int y = 0; y < Dim[1]; y++) {
                        double val = Image -> GetScalarComponentAsDouble(x, y, z, 0);
                        if (val < block_min) block_min = val;
                        if (val > block_max) block_max = val;
                    }
                }
            }
            
            // Use hybrid approach: local range but constrained by global range
            double block_range = block_max - block_min;
            double global_range_size = global_range[1] - global_range[0];
            
            // If block range is too small compared to global, use global stats
            if (block_range < global_range_size * 0.1) {
                block_min = global_range[0];
                block_max = global_range[1];
                block_range = global_range_size;
            }
            
                    // Debug output removed
            
            // Normalize this z-block
            if (block_range > 0) {
                for (int z = z_start; z < z_end; z++) {
                    for (int x = 0; x < Dim[0]; x++) {
                        for (int y = 0; y < Dim[1]; y++) {
                            double point[3] = {(double)x, (double)y, (double)z};
                            vtkIdType id = Image -> FindPoint(point);
                            double val = ScalarsShort -> GetTuple1(id);
                            double normalized = 255.0 * (val - block_min) / block_range;
                            ScalarsChar -> SetTuple1(id, (unsigned char)normalized);
                        }
                    }
                }
            } else {
                // If block has uniform intensity, use global normalization
                for (int z = z_start; z < z_end; z++) {
                    for (int x = 0; x < Dim[0]; x++) {
                        for (int y = 0; y < Dim[1]; y++) {
                            double point[3] = {(double)x, (double)y, (double)z};
                            vtkIdType id = Image -> FindPoint(point);
                            double val = ScalarsShort -> GetTuple1(id);
                            double normalized = 255.0 * (val - global_range[0]) / global_range_size;
                            ScalarsChar -> SetTuple1(id, (unsigned char)normalized);
                        }
                    }
                }
            }
        }
        
        ScalarsChar -> Modified();
        Image8 -> GetPointData() -> SetScalars(ScalarsChar);
        return Image8;

    } else {
        return NULL;
    }
}

vtkSmartPointer<vtkImageData> Convert16To8bitZAdaptiveGentle(vtkSmartPointer<vtkImageData> Image, int block_size) {

    // 8-Bit images
    if (Image -> GetScalarType() == VTK_UNSIGNED_CHAR) {

        return Image;

    // 16-Bit images
    } else if (Image -> GetScalarType() == VTK_UNSIGNED_SHORT) {

            // Debug output removed

        int *Dim = Image -> GetDimensions();
        vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
        Image8 -> ShallowCopy(Image);

        vtkDataArray *ScalarsShort = Image -> GetPointData() -> GetScalars();
        unsigned long int N = ScalarsShort -> GetNumberOfTuples();
        
        // First apply global normalization to preserve 3D relationships
        double global_range[2];
        ScalarsShort -> GetRange(global_range);
        
        vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
        ScalarsChar -> SetNumberOfComponents(1);
        ScalarsChar -> SetNumberOfTuples(N);
        
        // Apply global normalization first
        double global_scale = 255.0 / (global_range[1] - global_range[0]);
        for (vtkIdType id = 0; id < N; id++) {
            double val = ScalarsShort -> GetTuple1(id);
            double normalized = global_scale * (val - global_range[0]);
            ScalarsChar -> SetTuple1(id, (unsigned char)normalized);
        }
        
        // Then apply local contrast enhancement per z-block (using block_size)
        for (int z_start = 0; z_start < Dim[2]; z_start += block_size) {
            
            int z_end = (z_start + block_size < Dim[2]) ? z_start + block_size : Dim[2];
            
            // Calculate statistics for this z-block
            double block_sum = 0.0, block_mean = 0.0;
            double block_min = 255.0, block_max = 0.0;
            int block_pixels = Dim[0] * Dim[1] * (z_end - z_start);
            
            // First pass: calculate statistics for the entire z-block
            for (int z = z_start; z < z_end; z++) {
                for (int x = 0; x < Dim[0]; x++) {
                    for (int y = 0; y < Dim[1]; y++) {
                        double point[3] = {(double)x, (double)y, (double)z};
                        vtkIdType id = Image -> FindPoint(point);
                        double val = ScalarsChar -> GetTuple1(id);
                        block_sum += val;
                        if (val < block_min) block_min = val;
                        if (val > block_max) block_max = val;
                    }
                }
            }
            block_mean = block_sum / block_pixels;
            
            // Apply adaptive contrast enhancement for all z-blocks
            double block_range = block_max - block_min;
            if (block_range > 0) { // Process all blocks, not just dimmer ones
                
                // Adaptive enhancement based on block brightness
                double enhancement_factor;
                if (block_mean < 20) {
                    enhancement_factor = 3.0; // Very aggressive for extremely dark blocks
                } else if (block_mean < 50) {
                    enhancement_factor = 2.5; // More aggressive for very dark blocks
                } else if (block_mean < 100) {
                    enhancement_factor = 1.5; // Gentle enhancement for moderately dark blocks
                } else if (block_mean < 150) {
                    enhancement_factor = 1.2; // Light enhancement for medium brightness blocks
                } else {
                    enhancement_factor = 1.1; // Minimal enhancement for bright blocks
                }
                
                // Apply enhancement to all pixels in this z-block
                for (int z = z_start; z < z_end; z++) {
                    for (int x = 0; x < Dim[0]; x++) {
                        for (int y = 0; y < Dim[1]; y++) {
                            double point[3] = {(double)x, (double)y, (double)z};
                            vtkIdType id = Image -> FindPoint(point);
                            double val = ScalarsChar -> GetTuple1(id);
                            
                            // Apply contrast enhancement around block mean
                            double enhanced = block_mean + enhancement_factor * (val - block_mean);
                            
                            // Clamp to valid range
                            if (enhanced < 0) enhanced = 0;
                            if (enhanced > 255) enhanced = 255;
                            
                            ScalarsChar -> SetTuple1(id, (unsigned char)enhanced);
                        }
                    }
                }
                
                // Debug output removed
            }
        }
        
        ScalarsChar -> Modified();
        Image8 -> GetPointData() -> SetScalars(ScalarsChar);
        return Image8;

    // Other depth
    } else {
        return NULL;
    }
}

vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToChar(vtkSmartPointer<vtkImageData> Image, double threshold) {

    vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
    Image8 -> ShallowCopy(Image);

    vtkDataArray *ScalarsDouble = Image -> GetPointData() -> GetScalars();
    unsigned long int N = ScalarsDouble -> GetNumberOfTuples();
    double range[2];
    ScalarsDouble -> GetRange(range);

    vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
    ScalarsChar -> SetNumberOfComponents(1);
    ScalarsChar -> SetNumberOfTuples(N);
        
    double x;

    if (threshold > 0) {
        for ( vtkIdType id = N; id--; ) {
            x = ScalarsDouble -> GetTuple1(id);
            if (x<=threshold) {
                ScalarsChar -> SetTuple1(id,0);
            } else {
                ScalarsChar -> SetTuple1(id,255);
            }
        }
    } else {
        for ( vtkIdType id = N; id--; ) {
            x = ScalarsDouble -> GetTuple1(id);
            ScalarsChar -> SetTuple1(id,(int)(255*(x-range[0])/(range[1]-range[0])));
        }        
    }
    ScalarsChar -> Modified();

    Image8 -> GetPointData() -> SetScalars(ScalarsChar);
    return Image8;

}

vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToCharZAdaptive(vtkSmartPointer<vtkImageData> Image, double base_threshold) {

    // Debug output removed

    int *Dim = Image -> GetDimensions();
    vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
    Image8 -> ShallowCopy(Image);

    vtkDataArray *ScalarsDouble = Image -> GetPointData() -> GetScalars();
    unsigned long int N = ScalarsDouble -> GetNumberOfTuples();

    vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
    ScalarsChar -> SetNumberOfComponents(1);
    ScalarsChar -> SetNumberOfTuples(N);
    
    // Process each z-plane independently
    for (int z = 0; z < Dim[2]; z++) {
        
        // Calculate statistics for this z-plane
        double z_sum = 0.0, z_mean = 0.0, z_std = 0.0;
        double z_min = DBL_MAX, z_max = DBL_MIN;
        int z_pixels = Dim[0] * Dim[1];
        
        // First pass: calculate mean and range
        for (int x = 0; x < Dim[0]; x++) {
            for (int y = 0; y < Dim[1]; y++) {
                double point[3] = {(double)x, (double)y, (double)z};
                vtkIdType id = Image -> FindPoint(point);
                double val = ScalarsDouble -> GetTuple1(id);
                z_sum += val;
                if (val < z_min) z_min = val;
                if (val > z_max) z_max = val;
            }
        }
        z_mean = z_sum / z_pixels;
        
        // Second pass: calculate standard deviation
        double var_sum = 0.0;
        for (int x = 0; x < Dim[0]; x++) {
            for (int y = 0; y < Dim[1]; y++) {
                double point[3] = {(double)x, (double)y, (double)z};
                vtkIdType id = Image -> FindPoint(point);
                double val = ScalarsDouble -> GetTuple1(id);
                var_sum += (val - z_mean) * (val - z_mean);
            }
        }
        z_std = sqrt(var_sum / z_pixels);
        
        // Calculate adaptive threshold for this z-plane
        // Use mean + std deviation as threshold, scaled by base_threshold
        double z_threshold = z_mean + (z_std * base_threshold * 2.0);
        
        // Ensure threshold is within reasonable bounds
        if (z_threshold > z_max) z_threshold = z_max * 0.8;
        if (z_threshold < z_min) z_threshold = z_min + (z_max - z_min) * 0.1;
        
                // Debug output removed
        
        // Apply threshold to this z-plane
        for (int x = 0; x < Dim[0]; x++) {
            for (int y = 0; y < Dim[1]; y++) {
                double point[3] = {(double)x, (double)y, (double)z};
                vtkIdType id = Image -> FindPoint(point);
                double val = ScalarsDouble -> GetTuple1(id);
                
                if (val <= z_threshold) {
                    ScalarsChar -> SetTuple1(id, 0);
                } else {
                    ScalarsChar -> SetTuple1(id, 255);
                }
            }
        }
    }
    
    ScalarsChar -> Modified();
    Image8 -> GetPointData() -> SetScalars(ScalarsChar);
    return Image8;
}

vtkSmartPointer<vtkImageData> BinarizeAndConvertDoubleToCharZAdaptiveConservative(vtkSmartPointer<vtkImageData> Image, double base_threshold, int z_block_size) {

    // Debug output removed

    int *Dim = Image -> GetDimensions();
    vtkSmartPointer<vtkImageData> Image8 = vtkImageData::New();
    Image8 -> ShallowCopy(Image);

    vtkDataArray *ScalarsDouble = Image -> GetPointData() -> GetScalars();
    unsigned long int N = ScalarsDouble -> GetNumberOfTuples();

    vtkSmartPointer<vtkUnsignedCharArray> ScalarsChar = vtkSmartPointer<vtkUnsignedCharArray>::New();
    ScalarsChar -> SetNumberOfComponents(1);
    ScalarsChar -> SetNumberOfTuples(N);
    
    // Calculate global threshold as reference
    double global_range[2];
    ScalarsDouble -> GetRange(global_range);
    double global_threshold = base_threshold;
    
    // Process z-planes in blocks to reduce noise sensitivity
    for (int z_start = 0; z_start < Dim[2]; z_start += z_block_size) {
        
        int z_end = (z_start + z_block_size < Dim[2]) ? z_start + z_block_size : Dim[2];
        
        // Calculate statistics for this z-block
        double block_sum = 0.0, block_mean = 0.0, block_std = 0.0;
        double block_min = DBL_MAX, block_max = DBL_MIN;
        int block_pixels = Dim[0] * Dim[1] * (z_end - z_start);
        
        // First pass: calculate mean and range for the block
        for (int z = z_start; z < z_end; z++) {
            for (int x = 0; x < Dim[0]; x++) {
                for (int y = 0; y < Dim[1]; y++) {
                    double point[3] = {(double)x, (double)y, (double)z};
                    vtkIdType id = Image -> FindPoint(point);
                    double val = ScalarsDouble -> GetTuple1(id);
                    block_sum += val;
                    if (val < block_min) block_min = val;
                    if (val > block_max) block_max = val;
                }
            }
        }
        block_mean = block_sum / block_pixels;
        
        // Second pass: calculate standard deviation
        double var_sum = 0.0;
        for (int z = z_start; z < z_end; z++) {
            for (int x = 0; x < Dim[0]; x++) {
                for (int y = 0; y < Dim[1]; y++) {
                    double point[3] = {(double)x, (double)y, (double)z};
                    vtkIdType id = Image -> FindPoint(point);
                    double val = ScalarsDouble -> GetTuple1(id);
                    var_sum += (val - block_mean) * (val - block_mean);
                }
            }
        }
        block_std = sqrt(var_sum / block_pixels);
        
        // Improved adaptive threshold that properly maps user's threshold to block statistics
        // The user's threshold should represent a percentile of the local intensity distribution
        
        double block_range = block_max - block_min;
        
        // Guard against empty or uniform blocks
        if (block_range < 1e-6) {
            // If block is uniform, use global threshold as-is
            block_range = 1.0;
        }
        
        // Enhanced threshold mapping with improved dark region sensitivity
        double brightness_factor = (block_mean - block_min) / block_range;
        double cv = (block_mean > 0) ? (block_std / block_mean) : 0.0;
        
        // Detect very dark regions that need special handling
        bool is_very_dark_region = (block_mean < (global_range[1] - global_range[0]) * 0.1) || (brightness_factor < 0.2);
        
        double adaptive_threshold;
        
        if (is_very_dark_region) {
            // Use old z-adaptive approach for very dark regions - more sensitive
            // Statistical threshold: mean + std scaled by user threshold
            adaptive_threshold = block_mean + (block_std * base_threshold * 2.0);
            
            // For extremely dark regions, be even more aggressive
            if (block_mean < (global_range[1] - global_range[0]) * 0.05) {
                adaptive_threshold = block_mean + (block_std * base_threshold * 1.5);
            }
            
            // Ensure we don't go below meaningful threshold for dark regions
            double dark_min_threshold = block_min + (block_range * 0.01); // Very low threshold
            if (adaptive_threshold < dark_min_threshold) adaptive_threshold = dark_min_threshold;
            
        } else {
            // Use improved mapping for normal/bright regions
            double user_threshold_mapped = block_min + (block_range * base_threshold);
            
            // Apply adaptive adjustment based on block characteristics
            double adaptive_adjustment = 0.0;
            
            // For high-noise blocks (CV > 0.5), be more conservative (increase threshold)
            // For low-noise blocks (CV < 0.3), be more sensitive (decrease threshold)
            if (cv > 0.5) {
                adaptive_adjustment = block_std * 0.2; // Less sensitive for noisy blocks
            } else if (cv < 0.3) {
                adaptive_adjustment = -block_std * 0.1; // More sensitive for clean blocks
            }
            
            // Additional adjustment for moderately dim regions
            if (brightness_factor < 0.5) {
                adaptive_adjustment -= block_std * 0.1;
            }
            
            adaptive_threshold = user_threshold_mapped + adaptive_adjustment;
        }
        
        // Set reasonable bounds based on region type
        double min_threshold, max_threshold;
        if (is_very_dark_region) {
            // More permissive bounds for dark regions
            min_threshold = block_min + (block_range * 0.01);
            max_threshold = block_mean + (block_std * 3.0);
        } else {
            // Standard bounds for normal regions
            min_threshold = block_min + (block_range * (base_threshold * 0.3));
            max_threshold = block_min + (block_range * (base_threshold * 3.0));
        }
        
        if (adaptive_threshold < min_threshold) adaptive_threshold = min_threshold;
        if (adaptive_threshold > max_threshold) adaptive_threshold = max_threshold;
        
        // Debug output removed
        
        // Apply threshold to this z-block
        for (int z = z_start; z < z_end; z++) {
            for (int x = 0; x < Dim[0]; x++) {
                for (int y = 0; y < Dim[1]; y++) {
                    double point[3] = {(double)x, (double)y, (double)z};
                    vtkIdType id = Image -> FindPoint(point);
                    double val = ScalarsDouble -> GetTuple1(id);
                    
                    if (val <= adaptive_threshold) {
                        ScalarsChar -> SetTuple1(id, 0);
                    } else {
                        ScalarsChar -> SetTuple1(id, 255);
                    }
                }
            }
        }
    }
    
    ScalarsChar -> Modified();
    Image8 -> GetPointData() -> SetScalars(ScalarsChar);
    return Image8;
}

// Enhanced structural connectivity function with advanced algorithms
vtkSmartPointer<vtkImageData> EnhanceStructuralConnectivity(vtkSmartPointer<vtkImageData> Image, double sigma = 1.0) {
    #ifdef DEBUG
        printf("Applying strong structural connectivity enhancement...\n");
    #endif
    
    int *Dim = Image -> GetDimensions();
    vtkDataArray *OriginalScalars = Image -> GetPointData() -> GetScalars();
    unsigned long int N = OriginalScalars -> GetNumberOfTuples();
    
    // 1. Multi-scale Gaussian smoothing to connect structures at different distances
    vtkSmartPointer<vtkImageGaussianSmooth> GaussSmooth1 = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    GaussSmooth1 -> SetInputData(Image);
    GaussSmooth1 -> SetDimensionality(3);
    GaussSmooth1 -> SetStandardDeviations(sigma, sigma, sigma * 0.3);
    GaussSmooth1 -> Update();
    
    vtkSmartPointer<vtkImageGaussianSmooth> GaussSmooth2 = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    GaussSmooth2 -> SetInputData(Image);
    GaussSmooth2 -> SetDimensionality(3);
    GaussSmooth2 -> SetStandardDeviations(sigma * 1.8, sigma * 1.8, sigma * 0.6);
    GaussSmooth2 -> Update();
    
    vtkDataArray *SmoothedScalars1 = GaussSmooth1 -> GetOutput() -> GetPointData() -> GetScalars();
    vtkDataArray *SmoothedScalars2 = GaussSmooth2 -> GetOutput() -> GetPointData() -> GetScalars();
    
    // 2. Create enhanced image
    vtkSmartPointer<vtkImageData> EnhancedImage = vtkImageData::New();
    EnhancedImage -> ShallowCopy(Image);
    
    vtkSmartPointer<vtkDoubleArray> EnhancedScalars = vtkSmartPointer<vtkDoubleArray>::New();
    EnhancedScalars -> SetNumberOfComponents(1);
    EnhancedScalars -> SetNumberOfTuples(N);
    
    // 3. Advanced gap bridging algorithm
    for (int z = 1; z < Dim[2]-1; z++) {
        for (int y = 1; y < Dim[1]-1; y++) {
            for (int x = 1; x < Dim[0]-1; x++) {
                double point[3] = {(double)x, (double)y, (double)z};
                vtkIdType id = Image -> FindPoint(point);
                
                double original_val = OriginalScalars -> GetTuple1(id);
                double smooth1_val = SmoothedScalars1 -> GetTuple1(id);
                double smooth2_val = SmoothedScalars2 -> GetTuple1(id);
                
                // Check for strong signals in neighborhood
                double max_neighbor = 0.0;
                double neighbor_sum = 0.0;
                int strong_neighbors = 0;
                
                // Check 26-neighborhood
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0 && dz == 0) continue;
                            
                            double neighbor_point[3] = {(double)(x+dx), (double)(y+dy), (double)(z+dz)};
                            vtkIdType neighbor_id = Image -> FindPoint(neighbor_point);
                            double neighbor_val = OriginalScalars -> GetTuple1(neighbor_id);
                            
                            max_neighbor = (neighbor_val > max_neighbor) ? neighbor_val : max_neighbor;
                            neighbor_sum += neighbor_val;
                            if (neighbor_val > 0.1) strong_neighbors++;
                        }
                    }
                }
                
                double neighbor_avg = neighbor_sum / 26.0;
                
                // Basic enhancement combination
                double enhanced_val = original_val * 0.7 + smooth1_val * 0.2 + smooth2_val * 0.1;
                
                // Advanced gap bridging: if current position is weak but surrounded by strong neighbors
                if (original_val < 0.05 && strong_neighbors >= 4 && max_neighbor > 0.15) {
                    // This might be a gap that needs bridging
                    enhanced_val = neighbor_avg * 0.6 + smooth2_val * 0.4;
                }
                
                // Connection enhancement: if smoothed signal is stronger than original
                if (smooth2_val > original_val * 1.5 && smooth2_val > 0.08) {
                    enhanced_val = original_val * 0.4 + smooth2_val * 0.6;
                }
                
                // Strengthen existing strong signals
                if (original_val > 0.2) {
                    enhanced_val = original_val * 0.9 + smooth1_val * 0.1;
                }
                
                EnhancedScalars -> SetTuple1(id, enhanced_val);
            }
        }
    }
    
    // 4. Special handling for boundary regions
    for (int z = 0; z < Dim[2]; z++) {
        for (int y = 0; y < Dim[1]; y++) {
            for (int x = 0; x < Dim[0]; x++) {
                if (x == 0 || x == Dim[0]-1 || y == 0 || y == Dim[1]-1 || z == 0 || z == Dim[2]-1) {
                    double point[3] = {(double)x, (double)y, (double)z};
                    vtkIdType id = Image -> FindPoint(point);
                    double original_val = OriginalScalars -> GetTuple1(id);
                    EnhancedScalars -> SetTuple1(id, original_val);
                }
            }
        }
    }
    
    EnhancedScalars -> Modified();
    EnhancedImage -> GetPointData() -> SetScalars(EnhancedScalars);
    
    #ifdef DEBUG
        printf("Strong structural connectivity enhancement completed.\n");
    #endif
    
    return EnhancedImage;
}

// Connect fragmented skeleton segments
vtkSmartPointer<vtkPolyData> ConnectSkeletonFragments(vtkSmartPointer<vtkPolyData> Skeleton, double max_gap_distance) {
    #ifdef DEBUG
        printf("Connecting fragmented skeleton segments...\n");
    #endif
    
    // 1. Find all endpoints
    std::vector<vtkIdType> endpoints;
    vtkSmartPointer<vtkPolyData> ConnectedSkeleton = vtkSmartPointer<vtkPolyData>::New();
    ConnectedSkeleton -> DeepCopy(Skeleton);
    
    // Build connectivity relationships
    ConnectedSkeleton -> BuildLinks();
    
    // Find endpoints (degree-1 points)
    for (vtkIdType pointId = 0; pointId < ConnectedSkeleton->GetNumberOfPoints(); pointId++) {
        vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();
        ConnectedSkeleton->GetPointCells(pointId, cellIds);
        
        // Calculate how many edges this point connects to
        int degree = 0;
        for (vtkIdType i = 0; i < cellIds->GetNumberOfIds(); i++) {
            vtkCell* cell = ConnectedSkeleton->GetCell(cellIds->GetId(i));
            if (cell->GetPointId(0) == pointId || cell->GetPointId(cell->GetNumberOfPoints()-1) == pointId) {
                degree++;
            }
        }
        
        if (degree == 1) {
            endpoints.push_back(pointId);
        }
    }
    
    #ifdef DEBUG
        printf("\tFound %d endpoints\n", (int)endpoints.size());
    #endif
    
    // 2. Create connections for endpoint pairs
    std::vector<std::pair<vtkIdType, vtkIdType>> connections_to_add;
    
    for (size_t i = 0; i < endpoints.size(); i++) {
        for (size_t j = i + 1; j < endpoints.size(); j++) {
            vtkIdType point1 = endpoints[i];
            vtkIdType point2 = endpoints[j];
            
            double p1[3], p2[3];
            ConnectedSkeleton->GetPoint(point1, p1);
            ConnectedSkeleton->GetPoint(point2, p2);
            
            // Calculate Euclidean distance
            double distance = sqrt(
                (p1[0] - p2[0]) * (p1[0] - p2[0]) +
                (p1[1] - p2[1]) * (p1[1] - p2[1]) +
                (p1[2] - p2[2]) * (p1[2] - p2[2])
            );
            
            // If distance is within reasonable range, mark for connection
            if (distance <= max_gap_distance && distance > 0.1) {
                // Check if these two points are already in the same connected component
                bool already_connected = false;
                
                // Simple check: if reachable through existing edges, don't add new connection
                // (Simplified handling, should do proper connectivity check)
                
                if (!already_connected) {
                    connections_to_add.push_back(std::make_pair(point1, point2));
                }
            }
        }
    }
    
    #ifdef DEBUG
        printf("\tAdding %d connections\n", (int)connections_to_add.size());
    #endif
    
    // 3. Add new connections
    vtkSmartPointer<vtkCellArray> newLines = vtkSmartPointer<vtkCellArray>::New();
    
    // Copy existing line segments
    for (vtkIdType cellId = 0; cellId < ConnectedSkeleton->GetNumberOfCells(); cellId++) {
        vtkCell* cell = ConnectedSkeleton->GetCell(cellId);
        newLines->InsertNextCell(cell);
    }
    
    // Add new connection line segments
    for (const auto& connection : connections_to_add) {
        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, connection.first);
        line->GetPointIds()->SetId(1, connection.second);
        newLines->InsertNextCell(line);
    }
    
    // 4. Create new PolyData
    vtkSmartPointer<vtkPolyData> ResultSkeleton = vtkSmartPointer<vtkPolyData>::New();
    ResultSkeleton->SetPoints(ConnectedSkeleton->GetPoints());
    ResultSkeleton->SetLines(newLines);
    
    // Copy point data
    ResultSkeleton->GetPointData()->DeepCopy(ConnectedSkeleton->GetPointData());
    
    #ifdef DEBUG
        printf("Skeleton fragment connection completed.\n");
    #endif
    
    return ResultSkeleton;
}

void FillHoles(vtkSmartPointer<vtkImageData> ImageData) {

    #ifdef DEBUG
        printf("\tSearching for holes in the image...\n");
    #endif

    int *Dim = ImageData -> GetDimensions();
    vtkIdType N = ImageData -> GetNumberOfPoints();

    vtkIdType i, s, ido, id;

    int x, y, z;
    double v, r[3];
    bool find = true;
    long long int ro[3];
    long int scluster, label;
    ro[0] = Dim[0] * Dim[1] * Dim[2];
    ro[1] = Dim[0] * Dim[1];

    vtkSmartPointer<vtkIdList> CurrA = vtkSmartPointer<vtkIdList>::New();
    vtkSmartPointer<vtkIdList> NextA = vtkSmartPointer<vtkIdList>::New();
    vtkSmartPointer<vtkTypeInt64Array> CSz = vtkSmartPointer<vtkTypeInt64Array>::New();
    vtkSmartPointer<vtkTypeInt64Array> Volume = vtkSmartPointer<vtkTypeInt64Array>::New();
    Volume -> SetNumberOfComponents(1);
    Volume -> SetNumberOfTuples(N);
    Volume -> FillComponent(0,0);

    for (x = 1; x < Dim[0]-1; x++) {
        for (y = 1; y < Dim[1]-1; y++) {
            for (z = 1; z < Dim[2]-1; z++) {
                double point[3] = {(double)x, (double)y, (double)z};
                id = ImageData -> FindPoint(point);
                if ((unsigned short int)ImageData->GetScalarComponentAsDouble(x,y,z,0)) {
                    Volume -> SetTuple1(id,0);
                } else {
                    Volume -> SetTuple1(id,1);
                }
            }
        }
    }

    Volume -> Modified();

    label = 0;
    while (find) {
        for (s = 0; s < CurrA->GetNumberOfIds(); s++) {
            ido = CurrA -> GetId(s);
            ImageData -> GetPoint(ido,r);
            x = (int)r[0]; y = (int)r[1]; z = (int)r[2];
            for (i = 0; i < 6; i++) {
                double point[3] = {(double)(x+ssdx_sort[i]), (double)(y+ssdy_sort[i]), (double)(z+ssdz_sort[i])};
                id = ImageData -> FindPoint(point);
                v = Volume -> GetTuple1(id);
                if ((long int)v > 0) {
                    NextA -> InsertNextId(id);
                    Volume -> SetTuple1(id,-label);
                    scluster++;
                }
            }
        }
        if (!NextA->GetNumberOfIds()) {
            find = false;
            for (id=ro[0]; id--;) {
                v = Volume -> GetTuple1(id);
                if ((long int)v > 0) {
                    find = true;
                    ro[0] = id;
                    break;
                }
            }
            if (label) {
                CSz -> InsertNextTuple1(scluster);
            }
            if (find) {
                label++;
                scluster = 1;
                Volume -> SetTuple1(id,-label);
                CurrA -> InsertNextId(id);
            }
        } else {
            CurrA -> Reset();
            CurrA -> DeepCopy(NextA);
            NextA -> Reset();
        }
    }

    for (id = N; id--;) {
        if ((long int)Volume->GetTuple1(id)<-1) {
            ImageData -> GetPointData() -> GetScalars() -> SetTuple1(id,255);
        }
    }
    ImageData -> GetPointData() -> GetScalars() -> Modified();

    #ifdef DEBUG
        printf("\tNumber of filled holes: %ld\n",(long int)CSz->GetNumberOfTuples()-1);
    #endif

}

/* ================================================================
   ROUTINES FOR VESSELNESS CALCUATION VIA DISCRETE APPROCH
=================================================================*/

void GetImageDerivativeDiscrete(vtkSmartPointer<vtkDataArray> Image, int *dim, char direction, vtkSmartPointer<vtkFloatArray> Derivative) {
    // Debug output removed

    double d, f1, f2;
    vtkIdType i, j, k;
    if (direction=='x') {
        for (i = (vtkIdType)dim[0]; i--;) {
            for (j = (vtkIdType)dim[1]; j--;) {
                for (k = (vtkIdType)dim[2]; k--;) {
                    if (i==0) {
                        Image -> GetTuple(GetId(1,j,k,dim),&f1);
                        Image -> GetTuple(GetId(0,j,k,dim),&f2);
                    }
                    if (i==dim[0]-1) {
                        Image -> GetTuple(GetId(dim[0]-1,j,k,dim),&f1);
                        Image -> GetTuple(GetId(dim[0]-2,j,k,dim),&f2);
                    }
                    if (i>0&i<dim[0]-1) {
                        Image -> GetTuple(GetId(i+1,j,k,dim),&f1);
                        Image -> GetTuple(GetId(i-1,j,k,dim),&f2);
                        f1 /= 2.0; f2 /= 2.0;
                    }
                    d = f1 - f2;
                    Derivative -> SetTuple(GetId(i,j,k,dim),&d);
                }
            }
        }
    }

    if (direction=='y') {
        for (i = (vtkIdType)dim[0]; i--;) {
            for (j = (vtkIdType)dim[1]; j--;) {
                for (k = (vtkIdType)dim[2]; k--;) {
                    if (j==0) {
                        Image -> GetTuple(GetId(i,1,k,dim),&f1);
                        Image -> GetTuple(GetId(i,0,k,dim),&f2);
                    }
                    if (j==dim[1]-1) {
                        Image -> GetTuple(GetId(i,dim[1]-1,k,dim),&f1);
                        Image -> GetTuple(GetId(i,dim[1]-2,k,dim),&f2);
                    }
                    if (j>0&j<dim[1]-1) {
                        Image -> GetTuple(GetId(i,j+1,k,dim),&f1);
                        Image -> GetTuple(GetId(i,j-1,k,dim),&f2);
                        f1 /= 2.0; f2 /= 2.0;
                    }
                    d = f1 - f2;
                    Derivative -> SetTuple(GetId(i,j,k,dim),&d);
                }
            }
        }
    }

    if (direction=='z') {
        for (i = (vtkIdType)dim[0]; i--;) {
            for (j = (vtkIdType)dim[1]; j--;) {
                for (k = (vtkIdType)dim[2]; k--;) {
                    if (k==0) {
                        Image -> GetTuple(GetId(i,j,1,dim),&f1);
                        Image -> GetTuple(GetId(i,j,0,dim),&f2);
                    }
                    if (k==dim[2]-1) {
                        Image -> GetTuple(GetId(i,j,dim[2]-1,dim),&f1);
                        Image -> GetTuple(GetId(i,j,dim[2]-2,dim),&f2);
                    }
                    if (k>0&k<dim[2]-1) {
                        Image -> GetTuple(GetId(i,j,k+1,dim),&f1);
                        Image -> GetTuple(GetId(i,j,k-1,dim),&f2);
                        f1 /= 2.0; f2 /= 2.0;
                    }
                    d = f1 - f2;
                    Derivative -> SetTuple(GetId(i,j,k,dim),&d);
                }
            }
        }
    }

}

void GetHessianEigenvaluesDiscrete(double sigma, vtkSmartPointer<vtkImageData> Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3) {
    // Debug output removed

    int *Dim = Image -> GetDimensions();
    vtkIdType id, N = Image -> GetNumberOfPoints();
    double H[3][3], Eva[3], Eve[3][3], dxx, dyy, dzz, dxy, dxz, dyz, l1, l2, l3, frobnorm;

    // Debug output removed

    vtkSmartPointer<vtkImageGaussianSmooth> Gauss = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    Gauss -> SetInputData(Image);
    Gauss -> SetDimensionality(3);
    Gauss -> SetRadiusFactors(10,10,10);
    Gauss -> SetStandardDeviations(sigma,sigma,sigma);
    Gauss -> Update();

    vtkFloatArray *ImageG = (vtkFloatArray*) Gauss -> GetOutput() -> GetPointData() -> GetScalars();

    vtkSmartPointer<vtkFloatArray> Dx = vtkSmartPointer<vtkFloatArray>::New(); Dx -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dy = vtkSmartPointer<vtkFloatArray>::New(); Dy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dz = vtkSmartPointer<vtkFloatArray>::New(); Dz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxx = vtkSmartPointer<vtkFloatArray>::New(); Dxx -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dyy = vtkSmartPointer<vtkFloatArray>::New(); Dyy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dzz = vtkSmartPointer<vtkFloatArray>::New(); Dzz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxy = vtkSmartPointer<vtkFloatArray>::New(); Dxy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxz = vtkSmartPointer<vtkFloatArray>::New(); Dxz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dyz = vtkSmartPointer<vtkFloatArray>::New(); Dyz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Fro = vtkSmartPointer<vtkFloatArray>::New();
    Fro -> SetNumberOfComponents(1);
    Fro -> SetNumberOfTuples(N);

    GetImageDerivativeDiscrete(ImageG,Dim,'x',Dx);
    GetImageDerivativeDiscrete(ImageG,Dim,'y',Dy);
    GetImageDerivativeDiscrete(ImageG,Dim,'z',Dz);
    GetImageDerivativeDiscrete(Dx,Dim,'x',Dxx);
    GetImageDerivativeDiscrete(Dy,Dim,'y',Dyy);
    GetImageDerivativeDiscrete(Dz,Dim,'z',Dzz);
    GetImageDerivativeDiscrete(Dy,Dim,'x',Dxy);
    GetImageDerivativeDiscrete(Dz,Dim,'x',Dxz);
    GetImageDerivativeDiscrete(Dz,Dim,'y',Dyz);

    for ( id = N; id--; ) {
        l1 = l2 = l3 = 0.0;
        H[0][0]=Dxx->GetTuple1(id); H[0][1]=Dxy->GetTuple1(id); H[0][2]=Dxz->GetTuple1(id);
        H[1][0]=Dxy->GetTuple1(id); H[1][1]=Dyy->GetTuple1(id); H[1][2]=Dyz->GetTuple1(id);
        H[2][0]=Dxz->GetTuple1(id); H[2][1]=Dyz->GetTuple1(id); H[2][2]=Dzz->GetTuple1(id);
        frobnorm = FrobeniusNorm(H);
        if (H[0][0]+H[1][1]+H[2][2]<0.0) {
            vtkMath::Diagonalize3x3(H,Eva,Eve);
            l1 = Eva[0]; l2 = Eva[1]; l3 = Eva[2];
            Sort(&l1,&l2,&l3);
        }
        L1 -> SetTuple1(id,l1);
        L2 -> SetTuple1(id,l2);
        L3 -> SetTuple1(id,l3);
        Fro -> SetTuple1(id,frobnorm);
    }
    double ftresh,frobenius_norm_range[2];
    Fro -> GetRange(frobenius_norm_range);
    ftresh = sqrt(frobenius_norm_range[1]);

    for ( id = N; id--; ) {
        if ( Fro->GetTuple1(id) < ftresh) {
            L1 -> SetTuple1(id,0.0);
            L2 -> SetTuple1(id,0.0);
            L3 -> SetTuple1(id,0.0);
        }
    }
    L1 -> Modified();
    L2 -> Modified();
    L3 -> Modified();

}

void GetHessianEigenvaluesDiscreteZDependentThreshold(double sigma, vtkSmartPointer<vtkImageData> Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3, _mitoObject *mitoObject) {
    // Debug output removed

    int *Dim = Image -> GetDimensions();
    vtkIdType id, N = Image -> GetNumberOfPoints();
    double H[3][3], Eva[3], Eve[3][3], dxx, dyy, dzz, dxy, dxz, dyz, l1, l2, l3, frobnorm;

    // Debug output removed

    vtkSmartPointer<vtkImageGaussianSmooth> Gauss = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    Gauss -> SetInputData(Image);
    Gauss -> SetDimensionality(3);
    Gauss -> SetRadiusFactors(10,10,10);
    Gauss -> SetStandardDeviations(sigma,sigma,sigma);
    Gauss -> Update();

    vtkFloatArray *ImageG = (vtkFloatArray*) Gauss -> GetOutput() -> GetPointData() -> GetScalars();

    vtkSmartPointer<vtkFloatArray> Dx = vtkSmartPointer<vtkFloatArray>::New(); Dx -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dy = vtkSmartPointer<vtkFloatArray>::New(); Dy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dz = vtkSmartPointer<vtkFloatArray>::New(); Dz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxx = vtkSmartPointer<vtkFloatArray>::New(); Dxx -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dyy = vtkSmartPointer<vtkFloatArray>::New(); Dyy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dzz = vtkSmartPointer<vtkFloatArray>::New(); Dzz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxy = vtkSmartPointer<vtkFloatArray>::New(); Dxy -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dxz = vtkSmartPointer<vtkFloatArray>::New(); Dxz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Dyz = vtkSmartPointer<vtkFloatArray>::New(); Dyz -> SetNumberOfTuples(N);
    vtkSmartPointer<vtkFloatArray> Fro = vtkSmartPointer<vtkFloatArray>::New();
    Fro -> SetNumberOfComponents(1);
    Fro -> SetNumberOfTuples(N);

    GetImageDerivativeDiscrete(ImageG,Dim,'x',Dx);
    GetImageDerivativeDiscrete(ImageG,Dim,'y',Dy);
    GetImageDerivativeDiscrete(ImageG,Dim,'z',Dz);
    GetImageDerivativeDiscrete(Dx,Dim,'x',Dxx);
    GetImageDerivativeDiscrete(Dy,Dim,'y',Dyy);
    GetImageDerivativeDiscrete(Dz,Dim,'z',Dzz);
    GetImageDerivativeDiscrete(Dy,Dim,'x',Dxy);
    GetImageDerivativeDiscrete(Dz,Dim,'x',Dxz);
    GetImageDerivativeDiscrete(Dz,Dim,'y',Dyz);

    int x, y, z;
    int nblks = mitoObject->_nblks;

    double ***FThresh = new double**[nblks];
    for ( int qx = nblks; qx--; ) {
        FThresh[qx] = new double*[nblks];
        for ( int qy = nblks; qy--; ) {
            FThresh[qx][qy] = new double[Dim[2]];
            for ( id = Dim[2]; id--; ) {
                FThresh[qx][qy][id] = 0.0;
            }
        }
    }

    for ( id = N; id--; ) {
        l1 = l2 = l3 = 0.0;
        H[0][0]=Dxx->GetTuple1(id); H[0][1]=Dxy->GetTuple1(id); H[0][2]=Dxz->GetTuple1(id);
        H[1][0]=Dxy->GetTuple1(id); H[1][1]=Dyy->GetTuple1(id); H[1][2]=Dyz->GetTuple1(id);
        H[2][0]=Dxz->GetTuple1(id); H[2][1]=Dyz->GetTuple1(id); H[2][2]=Dzz->GetTuple1(id);
        frobnorm = FrobeniusNorm(H);
        if (H[0][0]+H[1][1]+H[2][2]<0.0) {
            vtkMath::Diagonalize3x3(H,Eva,Eve);
            l1 = Eva[0]; l2 = Eva[1]; l3 = Eva[2];
            Sort(&l1,&l2,&l3);
        }
        L1 -> SetTuple1(id,l1);
        L2 -> SetTuple1(id,l2);
        L3 -> SetTuple1(id,l3);
        Fro -> SetTuple1(id,frobnorm);
        x = GetX(id,Dim);
        y = GetY(id,Dim);
        z = GetZ(id,Dim);
        FThresh[int((1.0*nblks*x)/Dim[0])][int((1.0*nblks*y)/Dim[1])][z] = (frobnorm > FThresh[int((1.0*nblks*x)/Dim[0])][int((1.0*nblks*y)/Dim[1])][z]) ? frobnorm : FThresh[int((1.0*nblks*x)/Dim[0])][int((1.0*nblks*y)/Dim[1])][z];
    }

    for ( z = Dim[2]; z--; ) {
        for ( x = nblks; x--; ) {
            for ( y = nblks; y--; ) {
                FThresh[x][y][z] = sqrt(FThresh[x][y][z]);
            }
        }
    }

    //
    // Testing Region-Based Threshold
    //
    
    int j;
    double frobneigh;
    for ( id = N; id--; ) {
        x = GetX(id,Dim);
        y = GetY(id,Dim);
        z = GetZ(id,Dim);
        frobneigh = 0.0;
        if (x>0&&x<Dim[0]-1&&y>0&&y<Dim[1]-1&&z>0&&z<Dim[2]-1) {
            for (j = 0; j < 6; j++) {
                frobneigh += Fro -> GetTuple1(GetId(x+ssdx_sort[j],y+ssdy_sort[j],z+ssdz_sort[j],Dim));
            }
            frobneigh /= 6.0;
        }
        if ( frobneigh < FThresh[int((1.0*nblks*x)/Dim[0])][int((1.0*nblks*y)/Dim[1])][z] ) {
            L1 -> SetTuple1(id,0.0);
            L2 -> SetTuple1(id,0.0);
            L3 -> SetTuple1(id,0.0);
        }
    }
    L1 -> Modified();
    L2 -> Modified();
    L3 -> Modified();

    for ( int qx = nblks; qx--; ) {
        for ( int qy = nblks; qy--; ) {
            delete[] FThresh[qx][qy];
        }
        delete[] FThresh[qx];
    }
    delete[] FThresh;

}

/* ================================================================
   VESSELNESS ROUTINE
=================================================================*/

void GetVesselness(double sigma, vtkSmartPointer<vtkImageData> Image, vtkSmartPointer<vtkDoubleArray> L1, vtkSmartPointer<vtkDoubleArray> L2, vtkSmartPointer<vtkDoubleArray> L3, _mitoObject *mitoObject) {

    // Debug output removed

    double c = 500.0;
    double beta = 0.5;
    double alpha = 0.5;
    double std = 2 * c * c;
    double rbd = 2 * beta * beta;
    double rad = 2 * alpha * alpha;
    double l1, l2, l3, ra, ran, rb, rbn, st, stn, ft_old, ft_new;
    vtkIdType id, N = Image -> GetNumberOfPoints();

    if (mitoObject->_adaptive_threshold) {
        GetHessianEigenvaluesDiscreteZDependentThreshold(sigma,Image,L1,L2,L3,mitoObject);
    } else {
        GetHessianEigenvaluesDiscrete(sigma,Image,L1,L2,L3);
    }
    
    for ( id = N; id--; ) {
        l1 = L1 -> GetTuple1(id);
        l2 = L2 -> GetTuple1(id);
        l3 = L3 -> GetTuple1(id);
        if (l2<0&&l3<0) {

            ra = fabs(l2) / fabs(l3);
            ran = -ra * ra;

            rb = fabs(l1) / sqrt(l2*l3);
            rbn = -rb * rb;

            st = sqrt(l1*l1+l2*l2+l3*l3);
            stn = -st * st;

            ft_new = (1-exp(ran/rad)) * exp(rbn/rbd) * (1-exp(stn/std));

            //L1 is used to return vesselness values
            L1 -> SetTuple1(id,ft_new);

        } else L1 -> SetTuple1(id,0.0);
    }
    L1 -> Modified();
}

/* ================================================================
   DIVERGENCE FILTER
=================================================================*/

void GetDivergenceFilter(int *Dim, vtkSmartPointer<vtkDoubleArray> Scalars) {

    #ifdef DEBUG
        printf("Calculating Divergent Filter...\n");
    #endif

    vtkIdType id;
    int j, i;
    int x, y, z, s = 2;
    double v, norm, V[6][3];
    int Dx[6] = {1,-1,0,0,0,0};
    int Dy[6] = {0,0,1,-1,0,0};
    int Dz[6] = {0,0,0,0,1,-1};
    int MI[3][3] = {{1,0,0},{0,1,0},{0,0,1}};

    vtkSmartPointer<vtkDoubleArray> Div = vtkSmartPointer<vtkDoubleArray>::New();
    Div -> SetNumberOfComponents(1);
    Div -> SetNumberOfTuples(Scalars->GetNumberOfTuples());
    Div -> FillComponent(0,0.0);

    for (z = s+1; z < Dim[2]-s-1; z++) {
        for (y = s+1; y < Dim[1]-s-1; y++) {
            for (x = s+1; x < Dim[0]-s-1; x++) {
                v = 0.0;
                id = GetId(x,y,z,Dim);
                if (Scalars->GetTuple1(id)) {
                    for (i = 0; i < 6; i++) {
                        for (j = 0; j < 3; j++) {
                            V[i][j]  = Scalars -> GetTuple1(GetId(x+s*Dx[i]+MI[j][0],y+s*Dy[i]+MI[j][1],z+s*Dz[i]+MI[j][2],Dim));
                            V[i][j] -= Scalars -> GetTuple1(GetId(x+s*Dx[i]-MI[j][0],y+s*Dy[i]-MI[j][1],z+s*Dz[i]-MI[j][2],Dim));
                        }
                        norm = sqrt(pow(V[i][0],2)+pow(V[i][1],2)+pow(V[i][2],2));
                        if (norm) {V[i][0]/=norm; V[i][1]/=norm; V[i][2]/=norm; }
                    }
                    v = (V[0][0]-V[1][0])+(V[2][1]-V[3][1])+(V[4][2]-V[5][2]);
                    v = (v<0) ? -v / 6.0 : 0.0;
                }
                Div -> InsertTuple1(id,v);
            }
        }
    }
    Div -> Modified();
    Scalars -> DeepCopy(Div);
    Scalars -> Modified();

}

/* ================================================================
   WIDTH ANALYSIS
=================================================================*/

void EstimateTubuleWidth(vtkSmartPointer<vtkPolyData> Skeleton, vtkSmartPointer<vtkPolyData> Surface, _mitoObject *mitoObject) {

    #ifdef DEBUG
        printf("Calculating tubules width...\n");
    #endif

    vtkIdType N = Skeleton -> GetNumberOfPoints();
    vtkSmartPointer<vtkDoubleArray> Width = vtkSmartPointer<vtkDoubleArray>::New();
    Width -> SetName("Width");
    Width -> SetNumberOfComponents(1);
    Width -> SetNumberOfTuples(N);

    #ifdef DEBUG
        printf("\tGenerating point locator...\n");
    #endif

    vtkSmartPointer<vtkKdTreePointLocator> Tree = vtkSmartPointer<vtkKdTreePointLocator>::New();
    Tree -> SetDataSet(Surface);
    Tree -> BuildLocator();

    int k, n = 3;
    vtkIdType id, idk;
    double r[3], rk[3], w;
    vtkSmartPointer<vtkIdList> List = vtkSmartPointer<vtkIdList>::New();
    
    double av_w = 0.0, sd_w = 0.0;

    for (id = 0; id < N; id++) {
        w = 0;
        Skeleton -> GetPoint(id,r);
        Tree -> FindClosestNPoints(n,r,List);
        for (k = 0; k < n; k++) {
            idk = List -> GetId(k);
            Surface -> GetPoint(idk,rk);
            w += 2.0*sqrt( pow(r[0]-rk[0],2) + pow(r[1]-rk[1],2) + pow(r[2]-rk[2],2) );
        }
        w /= n;
        av_w += w;
        sd_w += w*w;
        Width -> SetTuple1(id,w);

    }
    Width -> Modified();
    Skeleton -> GetPointData() -> SetScalars(Width);

    attribute newAtt_1 = {"Average width (um)",av_w / N};
    mitoObject -> attributes.push_back(newAtt_1);
    attribute newAtt_2 = {"Std width (um)",sqrt(sd_w/N - (av_w/N)*(av_w/N))};
    mitoObject -> attributes.push_back(newAtt_2);

}

/* ================================================================
   TUBULES LENGTH
=================================================================*/

void EstimateTubuleLength(vtkSmartPointer<vtkPolyData> Skeleton) {

    vtkIdType N = Skeleton -> GetNumberOfPoints();
    vtkSmartPointer<vtkDoubleArray> Length = vtkSmartPointer<vtkDoubleArray>::New();
    Length -> SetName("Length");
    Length -> SetNumberOfComponents(1);
    Length -> SetNumberOfTuples(N);

    double h, r1[3], r2[3], R1, R2, length;
    vtkPoints *Points = Skeleton -> GetPoints();
    for (vtkIdType edge=Skeleton->GetNumberOfCells();edge--;) {
        length = 0.0;
        for (vtkIdType n = 1; n < Skeleton->GetCell(edge)->GetNumberOfPoints(); n++) {
            Skeleton -> GetPoint(Skeleton->GetCell(edge)->GetPointId(n-1),r1);
            Skeleton -> GetPoint(Skeleton->GetCell(edge)->GetPointId(n  ),r2);
            h = sqrt(pow(r2[0]-r1[0],2)+pow(r2[1]-r1[1],2)+pow(r2[2]-r1[2],2));
            length += h;
        }
        for (vtkIdType n = 0; n < Skeleton->GetCell(edge)->GetNumberOfPoints(); n++) {
            Length -> SetTuple1(Skeleton->GetCell(edge)->GetPointId(n),length);
        }
    }

    Length -> Modified();
    Skeleton -> GetPointData() -> AddArray(Length);

}

/* ================================================================
   INTENSITY MAPPING
=================================================================*/

void MapImageIntensity(vtkSmartPointer<vtkPolyData> Skeleton, vtkSmartPointer<vtkImageData> ImageData, int nneigh) {

    int *Dim = ImageData -> GetDimensions();

    vtkIdType N = Skeleton -> GetNumberOfPoints();
    vtkSmartPointer<vtkDoubleArray> Intensity = vtkSmartPointer<vtkDoubleArray>::New();
    Intensity -> SetName("Intensity");
    Intensity -> SetNumberOfComponents(1);
    Intensity -> SetNumberOfTuples(N);

    vtkIdType id;
    int x, y, z, k;
    double v, r[3];
    for (id = 0; id < N; id++) {
        v = 0;
        Skeleton -> GetPoint(id,r);
        x = round(r[0] / _dxy);
        y = round(r[1] / _dxy);
        z = round(r[2] / _dz);
        for (k = 0; k < nneigh; k++) {
            if ((x+ssdx_sort[k]>=0)&&(y+ssdy_sort[k]>=0)&&(z+ssdz_sort[k]>=0)&&(x+ssdx_sort[k]<Dim[0])&&(y+ssdy_sort[k]<Dim[1])&&(z+ssdz_sort[k]<Dim[2]))
                v += ImageData -> GetScalarComponentAsDouble(x+ssdx_sort[k],y+ssdy_sort[k],z+ssdz_sort[k],0);
        }
        Intensity -> SetTuple1(id,v/((double)nneigh));
    }
    Intensity -> Modified();
    Skeleton -> GetPointData() -> AddArray(Intensity);

}

/* ================================================================
   WIDTH-CORRECTED VOLUME
=================================================================*/

void GetVolumeFromSkeletonLengthAndWidth(vtkSmartPointer<vtkPolyData> PolyData, _mitoObject *mitoObject) {
    double h, r1[3], r2[3], R1, R2, volume = 0.0, length = 0.0;
    vtkPoints *Points = PolyData -> GetPoints();
    for (vtkIdType edge=PolyData->GetNumberOfCells();edge--;) {
        for (vtkIdType n = 1; n < PolyData->GetCell(edge)->GetNumberOfPoints(); n++) {
            PolyData -> GetPoint(PolyData->GetCell(edge)->GetPointId(n-1),r1);
            PolyData -> GetPoint(PolyData->GetCell(edge)->GetPointId(n  ),r2);
            R1 = 0.5*PolyData -> GetPointData() -> GetArray("Width") -> GetTuple1(PolyData->GetCell(edge)->GetPointId(n-1));
            R2 = 0.5*PolyData -> GetPointData() -> GetArray("Width") -> GetTuple1(PolyData->GetCell(edge)->GetPointId(n  ));
            h = sqrt(pow(r2[0]-r1[0],2)+pow(r2[1]-r1[1],2)+pow(r2[2]-r1[2],2));
            length += h;
            volume += 3.141592 / 3.0 * h * (R1*R1+R2*R2+R1*R2);
        }
    }
    attribute newAtt_1 = {"Total length (um)",length};
    mitoObject -> attributes.push_back(newAtt_1);
    attribute newAtt_2 = {"Volume from length (um3)",length * (acos(-1.0)*pow(_rad,2))};
    mitoObject -> attributes.push_back(newAtt_2);
}


/* ================================================================
   TOPOLOGICAL ATTRIBUTES FROM SKELETON
=================================================================*/

void GetTopologicalAttributes(vtkSmartPointer<vtkPolyData> PolyData, _mitoObject *mitoObject) {
    // NUMBER OF END POINTS
    long int n, k;
    std::list<vtkIdType> Endpoints;
    for (vtkIdType edge=PolyData->GetNumberOfCells();edge--;) {
        n = PolyData->GetCell(edge)->GetNumberOfPoints();
        Endpoints.push_back(PolyData->GetCell(edge)->GetPointId(0));
        Endpoints.push_back(PolyData->GetCell(edge)->GetPointId(n-1));
    }
    std::list<vtkIdType> Nodes = Endpoints;
    Nodes.sort();
    Nodes.unique();
    // NUMBER OF BIFURCATION POINTS
    long int ne = 0, nb = 0;
    for (std::list<vtkIdType>::iterator it = Nodes.begin(); it!=Nodes.end(); it++) {
        k = std::count(Endpoints.begin(),Endpoints.end(),*it);
        ne += (k==1) ? 1 : 0;
        nb += (k>=3) ? 1 : 0;
    }
    attribute newAtt_1 = {"#End points",static_cast<double>(ne)};
    mitoObject -> attributes.push_back(newAtt_1);
    attribute newAtt_2 = {"#Bifurcations",static_cast<double>(nb)};
    mitoObject -> attributes.push_back(newAtt_2);
    // NUMBER OF CONNECTED COMPONENTS
    vtkSmartPointer<vtkPolyDataConnectivityFilter> CC = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    CC -> SetInputData(PolyData);
    CC -> ColorRegionsOn();
    CC -> Update();
    vtkSmartPointer<vtkPolyData> CCPolyData = CC -> GetOutput();
    double range[2];
    CCPolyData->GetPointData()->GetArray("RegionId")->GetRange(range);
    attribute newAtt_3 = {"#CComps",range[1]+1};
    mitoObject -> attributes.push_back(newAtt_3);
}

/* ================================================================
   MULTISCALE VESSELNESS
=================================================================*/

int MultiscaleVesselness(_mitoObject *mitoObject) {     

    vtkIdType id;
    int x, y, z, *Dim;
    vtkSmartPointer<vtkImageData> Image;
    vtkSmartPointer<vtkTIFFReader> TIFFReader;
    vtkSmartPointer<vtkStructuredPointsReader> STRUCReader;

    if ( mitoObject->Type == "TIF" ) {

        // Loading multi-paged TIFF file (Supported by VTK 6.2 and higher)
        TIFFReader = vtkSmartPointer<vtkTIFFReader>::New();
        int errlog = TIFFReader -> CanReadFile((mitoObject->FileName+".tif").c_str());
        // File cannot be opened
        if ( !errlog ) {
            printf("File %s cannnot be opened.\n",(mitoObject->FileName+".tif").c_str());
            return -1;
        }
        TIFFReader -> SetFileName((mitoObject->FileName+".tif").c_str());
        TIFFReader -> Update();

        Dim = TIFFReader -> GetOutput() -> GetDimensions();

        // Exporting resampled images

        if ( _export_image_resampled ) {
            SaveImageData(TIFFReader->GetOutput(),(mitoObject->FileName+"_resampled.tif").c_str(),true);
        }

        if ( Dim[2] == 1 ) {

            double avg_bkgrd = SampleBackgroundIntensity(TIFFReader->GetOutput()->GetPointData()->GetScalars());

            #ifdef DEBUG
                printf("2D Image Detected...\n");
                printf("\tAvg Background Intensity: %1.3f\n",avg_bkgrd);
            #endif

            double v;
            int sz = 7;
            Image = vtkSmartPointer<vtkImageData>::New();
            Image -> SetDimensions(Dim[0],Dim[1],sz);
            Image -> SetSpacing(1,1,1);
            Image -> SetOrigin(0,0,0);
            
            vtkSmartPointer<vtkUnsignedShortArray> Scalar = vtkSmartPointer<vtkUnsignedShortArray>::New();
            Scalar -> SetNumberOfComponents(1);
            Scalar -> SetNumberOfTuples(Dim[0]*Dim[1]*sz);
            Scalar -> FillComponent(0,avg_bkgrd);

            for (x = 1; x < Dim[0]-1; x++) {
                for (y = 1; y < Dim[1]-1; y++) {
                    for (z = 0; z < sz; z++) {
                        if ( (z<2) || (z>sz-3) ) {
                            v = avg_bkgrd + 0.1*PoissonGen(avg_bkgrd);
                        } else {
                            v = TIFFReader -> GetOutput() -> GetScalarComponentAsDouble(x,y,0,0);
                        }
                        double point[3] = {(double)x, (double)y, (double)z};
                        id = Image -> FindPoint(point);
                        Scalar -> SetTuple1(id,v);
                    }
                }
            }

            Image -> GetPointData() -> SetScalars(Scalar);

            Image -> Modified();

        } else {

            if (_resample > 0) {

                vtkSmartPointer<vtkImageResample> Resample = vtkSmartPointer<vtkImageResample>::New();
                Resample -> SetInterpolationModeToLinear();
                Resample -> SetDimensionality(3);
                Resample -> SetInputData(TIFFReader->GetOutput());
                Resample -> SetAxisMagnificationFactor(0,1.0);
                Resample -> SetAxisMagnificationFactor(1,1.0);
                Resample -> SetAxisMagnificationFactor(2,_resample/_dxy);
                Resample -> Update();

                Image = Resample -> GetOutput();

                _dz = _dxy;
            
            } else {

                Image = TIFFReader -> GetOutput();

            }

        }

    } else if ( mitoObject->Type == "VTK" ){

        STRUCReader = vtkSmartPointer<vtkStructuredPointsReader>::New();
        STRUCReader -> SetFileName((mitoObject->FileName+"-mitovolume.vtk").c_str());
        STRUCReader -> Update();

        Image = STRUCReader -> GetOutput();

    } else {

        printf("Format not recognized.\n");
        return EXIT_FAILURE;

    }

    // Shifting the Stack to (0,0,0) and storing the
    // original origin in mitoObject->
    double *Origin = Image -> GetOrigin();
    mitoObject->Ox = Origin[0];
    mitoObject->Oy = Origin[1];
    mitoObject->Oz = Origin[2];
    Image -> SetOrigin(0,0,0);  

    // Conversion 16-bit to 8-bit
    // Z-adaptive and XY-adaptive are now independent options
    if (mitoObject->_z_adaptive) {
                // Apply gentle z-adaptive normalization that preserves some 3D continuity
        // Note: xy adaptive (mitoObject->_adaptive_threshold) remains independent
        Image = Convert16To8bitZAdaptiveGentle(Image, mitoObject->_z_block_size);
    } else {
        Image = Convert16To8bit(Image);
    }

    Dim = Image -> GetDimensions();

    #ifdef DEBUG
        printf("MitoGraph %s [DEBUG mode]\n",MITOGRAPH_VERSION.c_str());
        printf("File name: %s\n",mitoObject->FileName.c_str());
        printf("Volume dimensions: %dx%dx%d\n",Dim[0],Dim[1],Dim[2]);
        printf("Scales to run: [%1.3f:%1.3f:%1.3f]\n",mitoObject->_sigmai,mitoObject->_dsigma,mitoObject->_sigmaf);
        printf("Threshold: %1.5f\n",_div_threshold);
    #endif

    if (!Image) printf("Format not supported.\n");

    vtkSmartPointer<vtkImageData> Binary;
    vtkSmartPointer<vtkContourFilter> Filter = vtkSmartPointer<vtkContourFilter>::New();
    vtkIdType N = Image -> GetNumberOfPoints();

    if (!mitoObject->_binary_input) {

        //VESSELNESS
        //----------

        vtkSmartPointer<vtkDoubleArray> AUX1 = vtkSmartPointer<vtkDoubleArray>::New();
        vtkSmartPointer<vtkDoubleArray> AUX2 = vtkSmartPointer<vtkDoubleArray>::New();
        vtkSmartPointer<vtkDoubleArray> AUX3 = vtkSmartPointer<vtkDoubleArray>::New();
        vtkSmartPointer<vtkDoubleArray> VSSS = vtkSmartPointer<vtkDoubleArray>::New();

        AUX1 -> SetNumberOfTuples(N);
        AUX2 -> SetNumberOfTuples(N);
        AUX3 -> SetNumberOfTuples(N);
        VSSS -> SetNumberOfTuples(N);
        AUX1 -> FillComponent(0,0.0);
        AUX2 -> FillComponent(0,0.0);
        AUX3 -> FillComponent(0,0.0);
        VSSS -> FillComponent(0,0.0);

        double sigma, vn, vo;

        for ( sigma = mitoObject->_sigmai; sigma <= mitoObject->_sigmaf+0.5*mitoObject->_dsigma; sigma += mitoObject->_dsigma ) {
            
            #ifdef DEBUG
                printf("Running sigma = %1.3f\n",sigma);
            #endif
            
            GetVesselness(sigma,Image,AUX1,AUX2,AUX3,mitoObject);
            
            for ( id = N; id--; ) {
                vn = AUX1 -> GetTuple1(id);
                vo = VSSS -> GetTuple1(id);
                if ( vn > vo ) {
                    VSSS -> SetTuple1(id,vn);
                }
            }

        }
        VSSS -> Modified();

        #ifdef DEBUG
            vtkSmartPointer<vtkImageData> ImageVess = vtkSmartPointer<vtkImageData>::New();
            ImageVess -> ShallowCopy(Image);
            ImageVess -> GetPointData() -> SetScalars(VSSS);
            ImageVess -> SetDimensions(Dim);

            SaveImageData(ImageVess,(mitoObject->FileName+"_vess.tif").c_str());
        #endif

        //DIVERGENCE FILTER
        //-----------------

        GetDivergenceFilter(Dim,VSSS);

        vtkSmartPointer<vtkImageData> ImageEnhanced = vtkSmartPointer<vtkImageData>::New();
        ImageEnhanced -> ShallowCopy(Image);
        ImageEnhanced -> GetPointData() -> SetScalars(VSSS);
        ImageEnhanced -> SetDimensions(Dim);

        #ifdef DEBUG
            SaveImageData(BinarizeAndConvertDoubleToChar(ImageEnhanced,-1),(mitoObject->FileName+"_div.tif").c_str());
        #endif

        #ifdef DEBUG
            printf("Clear boundaries and removing tiny components...\n");
        #endif

        CleanImageBoundaries(ImageEnhanced);

        long int cluster;
        std::vector<long int> CSz;
        vtkSmartPointer<vtkDoubleArray> Volume = vtkSmartPointer<vtkDoubleArray>::New();
        Volume -> SetNumberOfComponents(1);
        Volume -> SetNumberOfTuples(N);
        Volume -> FillComponent(0,0);
        long int ncc = LabelConnectedComponents(ImageEnhanced,Volume,CSz,6,_div_threshold); // can use _mitoObj here

        if (ncc > 1 && mitoObject->_smart_component_filtering) {
            // Use user-specified component size, or automatic based on threshold sensitivity
            int min_component_size = mitoObject->_min_component_size;
            
            #ifdef DEBUG
                printf("\tRemoving components smaller than %d voxels...\n", min_component_size);
            #endif
            
            for (id = N; id--;) {
                cluster = (long int)Volume -> GetTuple1(id);
                if (cluster < 0) {
                    if (CSz[-cluster-1] <= min_component_size) {
                        ImageEnhanced -> GetPointData() -> GetScalars() -> SetTuple1(id,0);
                    }
                }
            }
        }

        //STRUCTURAL CONNECTIVITY ENHANCEMENT
        //-----------------------------------
        if (mitoObject->_enhance_connectivity) {
            #ifdef DEBUG
                printf("Enhancing structural connectivity before binarization...\n");
            #endif
            
            // Use stronger connectivity enhancement for sensitive threshold settings
            double enhancement_strength = (_div_threshold < 0.1) ? 2.0 : 1.5;
            ImageEnhanced = EnhanceStructuralConnectivity(ImageEnhanced, enhancement_strength);
        }

        //BINARIZATION
        //------------
        if (mitoObject->_z_adaptive) {
            Binary = BinarizeAndConvertDoubleToCharZAdaptiveConservative(ImageEnhanced, _div_threshold, mitoObject->_z_block_size);
        } else {
            Binary = BinarizeAndConvertDoubleToChar(ImageEnhanced,_div_threshold); // can use _mitoObj here
        }

        //FILLING HOLES
        //-------------
        if (_improve_skeleton_quality) FillHoles(Binary);

        // EXPORT SEGMENTED IMAGE
        // ----------------------
        if (_export_image_binary) {
            vtkSmartPointer<vtkTIFFWriter> tif_writer = vtkSmartPointer<vtkTIFFWriter>::New();
            tif_writer->SetInputData(Binary);
            tif_writer->SetFileName((mitoObject->FileName + "_binary.tif").c_str());
            tif_writer->Write();
        }

        //MAX PROJECTION
        //--------------

        ExportMaxProjection(Binary,(mitoObject->FileName+".png").c_str());

        //CREATING SURFACE POLYDATA
        //-------------------------
        Filter -> SetInputData(ImageEnhanced);
        Filter -> SetValue(1,_div_threshold);

    } else {

        Binary = vtkSmartPointer<vtkImageData>::New();
        Binary -> DeepCopy(Image);

        //CREATING SURFACE POLYDATA
        //-------------------------
        Filter -> SetInputData(Binary);
        Filter -> SetValue(1,0.5);

    }

    Filter -> Update();

    vtkSmartPointer<vtkPolyData> Surface = Filter -> GetOutput();
    ScalePolyData(Surface,mitoObject);

    //SAVING SURFACE
    //--------------

    SavePolyData(Surface,(mitoObject->FileName+"_mitosurface.vtk").c_str());


    //CONNECTED COMPONENTS FOR GRAPH ANALYSIS
    //---------------------------------------

    std::vector<long int> CSz;
    vtkSmartPointer<vtkTypeInt64Array> CCVolume = vtkSmartPointer<vtkTypeInt64Array>::New();
    if (mitoObject->_analyze) {

        CCVolume -> SetNumberOfComponents(1);
        CCVolume -> SetNumberOfTuples(N);
        CCVolume -> FillComponent(0,0);

        long int ncc = LabelConnectedComponents(Binary,CCVolume,CSz,26,0);

    }

    //SKELETONIZATION
    //---------------

    vtkSmartPointer<vtkPolyData> Skeleton = Thinning3D(Binary,mitoObject);

    //CLEANING POLUDATA
    vtkSmartPointer<vtkCleanPolyData> Clean = vtkSmartPointer<vtkCleanPolyData>::New();
    Clean -> SetInputData(Skeleton);
    Clean -> Update();

    Skeleton -> DeepCopy(Clean->GetOutput());

    //FRAGMENT CONNECTION
    //-------------------
    if (mitoObject->_enhance_connectivity) {
        #ifdef DEBUG
            printf("Applying skeleton fragment connection...\n");
        #endif
        
        // Adjust gap distance based on pixel size and threshold
        double gap_distance = 3.0 * _dxy; // Base gap distance
        if (_div_threshold < 0.1) {
            gap_distance = 5.0 * _dxy; // For sensitive settings, allow larger gaps
        }
        
        Skeleton = ConnectSkeletonFragments(Skeleton, gap_distance);
        
        // Clean up the skeleton after connection
        vtkSmartPointer<vtkCleanPolyData> CleanAfterConnection = vtkSmartPointer<vtkCleanPolyData>::New();
        CleanAfterConnection -> SetInputData(Skeleton);
        CleanAfterConnection -> Update();
        Skeleton -> DeepCopy(CleanAfterConnection->GetOutput());
    }

    //CONNECTED COMPONENTS FOR GRAPH ANALYSIS
    //---------------------------------------

    if (mitoObject->_analyze) {

        FILE *fvol = fopen((mitoObject->FileName+".cc").c_str(),"w");
        fprintf(fvol,"Node\tBelonging_CC\tVol_Of_Belonging_CC_From_Img_(um3)\n");

        double r[3];
        long int node_id, cc_id, cc_tmp;
        for (id = 0; id < Skeleton->GetNumberOfPoints(); id++) {
            node_id = (long int)Skeleton -> GetPointData() -> GetArray("Nodes") -> GetTuple1(id);
            if (node_id > -1) {
                Skeleton -> GetPoint(id,r);
                for (char i = 0; i < 6; i++) {
                    double point[3] = {(double)((int)r[0]+ssdx_sort[i]), (double)((int)r[1]+ssdy_sort[i]), (double)((int)r[2]+ssdz_sort[i])};
                    cc_id = (long int)CCVolume->GetTuple1(Binary -> FindPoint(point));
                    // printf("%d\t%d\n",(int)node_id,(int)cc_id);
                    if (cc_id < 0) break;
                }
                if (cc_id < 0) {
                    fprintf(fvol,"%d\t%d\t%1.5f\n",(int)(node_id),(int)std::abs(cc_id),CSz[-cc_id-1]*(_dxy*_dxy*_dz));
                } else {
                    // If the voxel falls off the binary structure we assign volume zero.
                    // This will not affect the final report of volume per cc, once we
                    // use the max() function to get the volume of a given component.
                    fprintf(fvol,"%d\t%d\t0.00000\n",(int)(node_id),(int)std::abs(cc_id));
                }
            }
        }

        fclose(fvol);

    }

    //TUBULES WIDTH
    //-------------

    ScalePolyData(Skeleton,mitoObject);

    EstimateTubuleWidth(Skeleton,Surface,mitoObject);

    EstimateTubuleLength(Skeleton);

    //INTENSITY PROFILE ALONG THE SKELETON
    //------------------------------------

    vtkSmartPointer<vtkImageData> ImageData;
    if ( mitoObject->Type == "TIF" ) {

        TIFFReader = vtkSmartPointer<vtkTIFFReader>::New();
        TIFFReader -> SetFileName((mitoObject->FileName+".tif").c_str());
        TIFFReader -> Update();
        ImageData = TIFFReader -> GetOutput();

    } else {

        STRUCReader = vtkSmartPointer<vtkStructuredPointsReader>::New();
        STRUCReader -> SetFileName((mitoObject->FileName+"-mitovolume.vtk").c_str());
        STRUCReader -> Update();
        ImageData = STRUCReader -> GetOutput();

    }

    // Shifting the Stack to (0,0,0)
    ImageData -> SetOrigin(0,0,0);

    MapImageIntensity(Skeleton,ImageData,6);

    vtkDataArray *W = Skeleton -> GetPointData() -> GetArray("Width");
    vtkDataArray *I = Skeleton -> GetPointData() -> GetArray("Intensity");

    vtkIdType p;
    double r[3];
    FILE *fw = fopen((mitoObject->FileName+".txt").c_str(),"w");
    fprintf(fw,"line_id\tpoint_id\tx\ty\tz\twidth_(um)\tpixel_intensity\n");
    for (vtkIdType edge = 0; edge < Skeleton -> GetNumberOfCells(); edge++) {
        for (vtkIdType id = 0; id < Skeleton -> GetCell(edge) -> GetNumberOfPoints(); id++) {
            p = Skeleton -> GetCell(edge) -> GetPointId(id);
            Skeleton -> GetPoint(p,r);
            fprintf(fw,"%d\t%d\t%1.5f\t%1.5f\t%1.5f\t%1.5f\t%1.5f\n",(int)edge,(int)id,r[0],r[1],r[2],W->GetTuple1(p),I->GetTuple1(p));
        }
    }
    fclose(fw);

    GetVolumeFromSkeletonLengthAndWidth(Skeleton,mitoObject); //Validation needed

    //GetTopologicalAttributes(Skeleton,mitoObject);

    //SAVING SKELETON
    //---------------

    SavePolyData(Skeleton,(mitoObject->FileName+"_skeleton.vtk").c_str());

    return 0;
}

/* ================================================================
   GRAPH ANALYSIS
=================================================================*/

void RunGraphAnalysis(std::string FileName) {

    //RUN R SCRIPT FOR GRAPH ANALYSIS
    //-------------------------------

    std::string cmd;
    cmd = "Rscript --vanilla GraphAnalyzer.R " + FileName;
    system(cmd.c_str());

}

/* ================================================================
   MAIN ROUTINE
=================================================================*/

int main(int argc, char *argv[]) {     

    int i;
    char _impath[256];
    sprintf(_impath,"");
    bool _vtk_input = false;

    _mitoObject mitoObject;

    mitoObject.Type = "TIF";
    mitoObject._analyze = false;
    mitoObject._binary_input = false;
    mitoObject._adaptive_threshold = false;
    mitoObject._nblks = 3;
    mitoObject._sigmai = 1.00;
    mitoObject._sigmaf = 1.50;
    mitoObject._nsigma = 6;
    mitoObject._z_adaptive = false;
    mitoObject._z_block_size = 8; // Improved default for better statistics
    mitoObject._enhance_connectivity = false; // Default off - user controlled
    mitoObject._smart_component_filtering = false; // Default off - user controlled  
    mitoObject._min_component_size = 5; // Default component size

    // Collecting input parameters
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i],"-vtk")) {
            _vtk_input = true;
            mitoObject.Type = "VTK";
        }
        if (!strcmp(argv[i],"-path")) {
            mitoObject.Folder = argv[i+1];
            mitoObject.Folder += "/";
            sprintf(_impath,"%s/",argv[i+1]);
        }
        if (!strcmp(argv[i],"-xy")) {
            _dxy = atof(argv[i+1]);
        }
        if (!strcmp(argv[i],"-z")) {
            _dz = atof(argv[i+1]);
        }
        if (!strcmp(argv[i],"-rad")) {
            _rad = atof(argv[i+1]);
        }
        if (!strcmp(argv[i],"-scales")) {
            mitoObject._sigmai = atof(argv[i+1]);
            mitoObject._sigmaf = atof(argv[i+2]);
            mitoObject._nsigma = atoi(argv[i+3]);
        }
        if (!strcmp(argv[i],"-adaptive")) {
            mitoObject._adaptive_threshold = true;
            mitoObject._nblks = atoi(argv[i+1]);
        }
        if (!strcmp(argv[i],"-z-adaptive")) {
            mitoObject._z_adaptive = true;
        }
        if (!strcmp(argv[i],"-z-block-size")) {
            mitoObject._z_block_size = atoi(argv[i+1]);
            // Basic validation - just ensure it's positive
            if (mitoObject._z_block_size < 1) {
                printf("Warning: z_block_size too small (%d), setting to minimum of 1\n", mitoObject._z_block_size);
                mitoObject._z_block_size = 1;
            }
        }
        if (!strcmp(argv[i],"-enhance-connectivity")) {
            mitoObject._enhance_connectivity = true;
        }
        if (!strcmp(argv[i],"-smart-component-filtering")) {
            mitoObject._smart_component_filtering = true;
            if (i+1 < argc && argv[i+1][0] != '-') {
                mitoObject._min_component_size = atoi(argv[i+1]);
                if (mitoObject._min_component_size < 1) {
                    printf("Warning: min_component_size too small (%d), setting to minimum of 1\n", mitoObject._min_component_size);
                    mitoObject._min_component_size = 1;
                }
            }
        }
        if (!strcmp(argv[i],"-threshold")) {
            _div_threshold = (double)atof(argv[i+1]);
        }
        if (!strcmp(argv[i],"-scale_off")) {
            _scale_polydata_before_save = false;
        }        
        if (!strcmp(argv[i],"-graph_off")) {
            _export_graph_files = false;
        }
        if (!strcmp(argv[i],"-labels_off")) {
            _export_nodes_label = false;
        }
        if (!strcmp(argv[i],"-checkonly")) {
            _checkonly = true;
        }
        if (!strcmp(argv[i],"-precision_off")) {
            _improve_skeleton_quality = false;
        }
        if (!strcmp(argv[i],"-export_image_resampled")) {
            _export_image_resampled = true;
        }
        if (!strcmp(argv[i], "-export_image_binary")) {
            _export_image_binary = true;
        }
        if (!strcmp(argv[i],"-binary")) {
            mitoObject._binary_input = true;
        }
        if (!strcmp(argv[i],"-resample")) {
            _resample = atof(argv[i+1]);
        }
        if (!strcmp(argv[i],"-analyze")) {
            mitoObject._analyze = true;
        }
    }

    if (_dz < 0) {

        printf("Please, use -xy and -z to provide the pixel size.\n");
        return -1;

    }

    std::vector<std::string> Files; // List of files to process

    if (_vtk_input) {

        ScanFolderForThisExtension(mitoObject.Folder,"-mitovolume.vtk",&Files);

    } else {

        ScanFolderForThisExtension(mitoObject.Folder,".tif",&Files);

    }    

    mitoObject._dsigma = (mitoObject._nsigma>1) ? (mitoObject._sigmaf-mitoObject._sigmai) / (mitoObject._nsigma-1) : mitoObject._sigmaf;

    // Debug output removed

    for (int i = 0; i < Files.size(); i++) {

        mitoObject.attributes.clear();
        mitoObject.FileName = Files[i];
        
        if ( _checkonly ) {

            ExportDetailedMaxProjection(&mitoObject);

        } else {

            MultiscaleVesselness(&mitoObject);
            DumpResults(mitoObject);
            ExportConfigFile(mitoObject);

        }

        if ( mitoObject._analyze ) {
            RunGraphAnalysis(Files[i]);
        }

    }

    // Debug output removed

    return 0;
}
