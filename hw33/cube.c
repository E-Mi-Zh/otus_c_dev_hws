#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#define W 600
#define H 600

/* Координаты текстур */
double textures[6][4][2] = {
    /* 1 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}},
    /* 2 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}},
    /* 3 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}},
    /* 4 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}},
    /* 5 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}},
    /* 6 */
    {{0.0, 0.0},
    {1.0, 0.0},
    {1.0, 1.0}, 
    {0.0, 1.0}}
};

/* Координаты граней куба */
double cube[6][4][3] = {
    /* 1 */
    {{-1.0, -1.0, -1.0},
    {-1.0, -1.0, 1.0},
    {-1.0, 1.0, 1.0},
    {-1.0, 1.0, -1.0}},
    /* 2 */
    {{-1.0, -1.0, -1.0},
    {-1.0, 1.0, -1.0},
    {1.0, 1.0, -1.0},
    {1.0, -1.0, -1.0}},
    /* 3 */
    {{-1.0, -1.0, -1.0},
    {1.0, -1.0, -1.0},
    {1.0, -1.0, 1.0},
    {-1.0, -1.0, 1.0}},
    /* 4 */
    {{-1.0, -1.0, 1.0},
    {1.0, -1.0, 1.0},
    {1.0, 1.0, 1.0},
    {-1.0, 1.0, 1.0}},
    /* 5 */
    {{-1.0, 1.0, -1.0},
    {-1.0, 1.0, 1.0},
    {1.0, 1.0, 1.0},
    {1.0, 1.0, -1.0}},
    /* 6 */
    {{1.0, -1.0, -1.0},
    {1.0, 1.0, -1.0},
    {1.0, 1.0, 1.0},
    {1.0, -1.0, 1.0}}
};

/* Рисуем куб */
void cube_draw()
{
    glDisable(GL_COLOR);
    glEnable(GL_TEXTURE_2D);

    for (int i = 0; i < 6; i++) {
        glBindTexture(GL_TEXTURE_2D, i + 1);
        glBegin(GL_QUADS);
        for (int j = 0; j < 4; j++) {
            glTexCoord2f(textures[i][j][0], textures[i][j][1]);
            glVertex3f(cube[i][j][0], cube[i][j][1], cube[i][j][2]);
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_COLOR);
}

/* Функция загрузки текстур в формате bmp*/
GLuint texture_load(GLuint texture, const char* filename, int width, int height)
{
    unsigned char* texture_data;
    unsigned char R, G, B;
    FILE* file;
    uint8_t offset;

    file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error opening texture file %s: %s. Exiting...", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    texture_data = (unsigned char*) malloc(width * height * 3);
    if (texture_data == NULL) {
        fprintf(stderr, "Error allocating memory for texture %s: %s. Exiting...", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Пропускаем заголовок bmp */
    fseek(file, 10, 0);
    fread(&offset, 1, 1, file);
    fseek(file, offset, 0);
    fread(texture_data, width * height * 3, 1, file);
    fclose(file);

    /* меняем порядок BGR на RGB */
    int index;
    for (int i = 0; i < width * height; i++) {
        index = i * 3;
        B = texture_data[index];
        G = texture_data[index+1];
        R = texture_data[index+2];
        texture_data[index] = R;
        texture_data[index+1] = G;
        texture_data[index+2] = B;
    }

    /* Создаём текстуру */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB, GL_UNSIGNED_BYTE, texture_data);

    free(texture_data);
    return EXIT_SUCCESS;
}

void display(void)
{
    static double x_angle = 0;
    static double y_angle = 0;
    static double z_angle = 0;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0, 0.0, -7.0);

    /* Поворачиваем куб */
    glRotatef(x_angle, 1.0, 0.0, 0.0);
    glRotatef(y_angle, 0.0, 1.0, 0.0);
    glRotatef(z_angle, 0.0, 0.0, 1.0);
    x_angle = x_angle + 0.2;
    y_angle = y_angle + 0.4;
    z_angle = z_angle + 0.6;

    cube_draw();

    glutSwapBuffers();
}

void process_keys(unsigned char key, int x, int y)
{
    (void) x;
    (void) y;

    switch(key) {
        /* 27 - ASCII код клавиши Escape */
        case 27:
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
        }
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE);    
    glutInitWindowSize(W, H);
    glutCreateWindow("Cube");

    /* Включить удаление задних граней */
    glEnable(GL_CULL_FACE);

    /* Загружаем текстуры */
    texture_load(1, "1.bmp", 300, 300);
    texture_load(2, "2.bmp", 300, 300);
    texture_load(3, "3.bmp", 300, 300);
    texture_load(4, "4.bmp", 300, 300);
    texture_load(5, "5.bmp", 300, 300);
    texture_load(6, "6.bmp", 300, 300);

    glMatrixMode(GL_PROJECTION);
    gluPerspective(30, W/H, 1, 10);
    glMatrixMode(GL_MODELVIEW);

    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutKeyboardFunc(process_keys);
    glutMainLoop();          
}
