#include <GL/glut.h>

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glVertex2f(-1, -1);
    glVertex2f(0, 1);
    glVertex2f(1, -1);
    glEnd();
    glutSwapBuffers();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutCreateWindow("GLUT Test");
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
