int recieve_sigint = 0
struct shared_data_struct {
  int lock;
  /* Other data structures stuff */
} *ms;
/* signal_handler is the disposition of SIGINT */
void signal_handler(int sig)
{
  recieve_sigint = 1; //record SIGINT happened
}
int some_function(int a)
{
  /* Manipulate data structure */
  if (recieve_sigint) {
    spin_lock(&ms->lock);
    recieve_sigint = 0;
    /* Manipulate data structure in response to SIGINT */
    spin_unlock(&ms->lock);
  }
return something;
}
